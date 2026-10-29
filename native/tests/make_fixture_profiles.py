"""Generate profiles only for isolated test DLLs; never used by production main.dll."""
from pathlib import Path
import hashlib
import struct
import sys


class PE:
    def __init__(self, path):
        self.raw = Path(path).read_bytes()
        nt = self.u32(0x3c)
        assert self.raw[nt:nt+4] == b'PE\0\0'
        count, size = self.u16(nt+6), self.u16(nt+20)
        optional = nt+24
        assert self.u16(optional) == 0x20b and 0 < count < 128
        table = optional+size
        self.sections = [struct.unpack_from('<IIII', self.raw, table+40*i+8) for i in range(count)]
        export = self.offset(self.u32(optional+112))
        self.function_table = self.u32(export+28)
        names, ordinals = self.u32(export+32), self.u32(export+36)
        number = self.u32(export+24)
        assert number < 20000
        self.exports = {}
        for index in range(number):
            name = self.offset(self.u32(self.offset(names)+4*index))
            end = self.raw.index(b'\0', name, min(name+2048, len(self.raw)))
            ordinal = self.u16(self.offset(ordinals)+2*index)
            self.exports[self.raw[name:end].decode('ascii')] = self.u32(self.offset(self.function_table)+4*ordinal)

    def u16(self, pos): return struct.unpack_from('<H', self.raw, pos)[0]
    def u32(self, pos): return struct.unpack_from('<I', self.raw, pos)[0]
    def offset(self, rva):
        for virtual_size, virtual, raw_size, raw in self.sections:
            if virtual <= rva < virtual+raw_size:
                return raw+rva-virtual
        raise ValueError('RVA outside file-backed sections')


def main():
    game, loader, output = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    game_pe, loader_pe = PE(game), PE(loader)
    entry = game_pe.exports['ffr_test_encounter']
    ctor, dtor = '??0CppUserModBase@RC@@QEAA@XZ', '??1CppUserModBase@RC@@UEAA@XZ'
    game_hash = hashlib.sha256(game_pe.raw).hexdigest()
    loader_hash = hashlib.sha256(loader_pe.raw).hexdigest()
    offset = game_pe.offset(entry)
    code_hash = hashlib.sha256(game_pe.raw[offset:offset+64]).hexdigest()
    output.mkdir(parents=True, exist_ok=True)
    decision_rva=game_pe.exports['ffr_test_decision'];decision_offset=game_pe.offset(decision_rva)
    decision_hash=hashlib.sha256(game_pe.raw[decision_offset:decision_offset+64]).hexdigest()
    (output/'good_decision_profile.hpp').write_text('#pragma once\n#include <cstddef>\n#include <cstdint>\nnamespace ffr::decision_profile {\n'
        +f'inline constexpr std::uintptr_t handler_rva={decision_rva};\ninline constexpr std::size_t handler_size=64;\n'
        +f'inline constexpr char handler_sha256[]="{decision_hash}";\n}}\n',encoding='utf8')
    abort_text='#pragma once\n#include <cstdint>\nnamespace ffr::abort_profile {\ninline constexpr bool opening_boundary_reviewed=true;\n'
    for function in ['tick','get_world','get_map','get_ui','end_battle']:
        rva=game_pe.exports['ffr_test_abort_'+function];start=game_pe.offset(rva)
        digest=hashlib.sha256(game_pe.raw[start:start+64]).hexdigest()
        abort_text+=f'inline constexpr std::uintptr_t {function}_rva={rva};\ninline constexpr char {function}_sha256[]="{digest}";\n'
    abort_text+=f'inline constexpr std::uintptr_t manager_vtable_rva={game_pe.exports["ffr_test_btl_vtable"]};\n}}\n'
    (output/'good_abort_profile.hpp').write_text(abort_text,encoding='utf8')
    (output/'unreviewed_abort_profile.hpp').write_text(abort_text.replace('opening_boundary_reviewed=true','opening_boundary_reviewed=false'),encoding='utf8')
    clone_rva=game_pe.exports['ffr_test_load_memory'];clone_start=game_pe.offset(clone_rva)
    import_text='#pragma once\n#include <cstdint>\nnamespace ffr::import_profile {\n'
    import_text+=f'inline constexpr std::uintptr_t load_memory_rva={clone_rva};\ninline constexpr char load_memory_sha256[]="{hashlib.sha256(game_pe.raw[clone_start:clone_start+64]).hexdigest()}";\n'
    import_text+=f'inline constexpr std::uintptr_t save_vtable_rva={game_pe.exports["ffr_test_save_vtable"]};\n'
    for key,name in [('loader_ready','ffr_test_loader_ready'),('is_rooted','ffr_test_object_rooted'),('add_root','ffr_test_object_add_root'),('remove_root','ffr_test_object_remove_root')]:
        import_text+=f'inline constexpr char {key}_name[]="{name}";\ninline constexpr std::uintptr_t {key}_rva={loader_pe.exports[name]};\n'
    (output/'good_import_profile.hpp').write_text(import_text+'}\n',encoding='utf8')
    field_text='#pragma once\n#include <cstdint>\nnamespace ffr::field_profile {\n'
    field_text+=f'inline constexpr std::uintptr_t map_storage_vtable_rva={game_pe.exports["ffr_test_map_storage_vtable"]};\n'
    for function,export in [('get_world','ffr_test_field_get_world'),('get_map','ffr_test_abort_get_map'),('get_player','ffr_test_field_get_player'),('get_map_id','ffr_test_field_get_map_id')]:
        rva=game_pe.exports[export];start=game_pe.offset(rva)
        field_text+=f'inline constexpr std::uintptr_t {function}_rva={rva};\ninline constexpr char {function}_sha256[]="{hashlib.sha256(game_pe.raw[start:start+64]).hexdigest()}";\n'
    (output/'good_field_profile.hpp').write_text(field_text+'}\n',encoding='utf8')
    clock_text='#pragma once\n#include <cstdint>\nnamespace ffr::playtime_profile {\n'
    for function in ['get_clock','get_seconds']:
        rva=game_pe.exports['ffr_test_playtime_'+function];start=game_pe.offset(rva)
        clock_text+=f'inline constexpr std::uintptr_t {function}_rva={rva};\ninline constexpr char {function}_sha256[]="{hashlib.sha256(game_pe.raw[start:start+64]).hexdigest()}";\n'
    (output/'good_playtime_profile.hpp').write_text(clock_text+'}\n',encoding='utf8')
    rng_text='#pragma once\n#include <cstdint>\n#include <cstddef>\nnamespace ffr::crt_profile {\n'
    rng_text+=f'inline constexpr wchar_t module_name[]=L"";\ninline constexpr char module_sha256[]="{game_hash}";\n'
    rng_text+=f'inline constexpr std::uintptr_t import_rva={game_pe.exports["ffr_test_rand_import"]};\n'
    for key,name in [('rand','ffr_test_crt_rand'),('helper','ffr_test_crt_helper')]:
        rva=game_pe.exports[name];start=game_pe.offset(rva)
        rng_text+=f'inline constexpr std::uintptr_t {key}_rva={rva};\ninline constexpr std::size_t {key}_size=64;\ninline constexpr char {key}_sha256[]="{hashlib.sha256(game_pe.raw[start:start+64]).hexdigest()}";\n'
    for key,name in [('ready','ffr_test_rng_ready'),('fast','ffr_test_rng_fast'),('data_slot','ffr_test_rng_data_slot'),('state_slot','ffr_test_rng_state_slot'),('getter','ffr_test_rng_getter')]:
        rng_text+=f'inline constexpr std::uintptr_t {key}_rva={game_pe.exports[name]};\n'
    (output/'good_crt_profile.hpp').write_text(rng_text+'}\n',encoding='utf8')
    engine_text='#pragma once\n#include <cstdint>\n#include <cstddef>\nnamespace ffr::engine_rng_profile {\n'
    engine_text+=f'inline constexpr std::uintptr_t seed_rva={game_pe.exports["ffr_test_engine_rng_seed"]};\n'
    for key,name in [('draw','ffr_test_engine_rng_draw'),('initialize','ffr_test_engine_rng_initialize')]:
        rva=game_pe.exports[name];start=game_pe.offset(rva)
        engine_text+=f'inline constexpr std::uintptr_t {key}_rva={rva};\ninline constexpr std::size_t {key}_size=64;\ninline constexpr char {key}_sha256[]="{hashlib.sha256(game_pe.raw[start:start+64]).hexdigest()}";\n'
    (output/'good_engine_rng_profile.hpp').write_text(engine_text+'}\n',encoding='utf8')
    extra_functions = {name: game_pe.exports['ffr_test_'+name] for name in ['root_getter','save_memory','memory_free']}
    for name in ['good', 'bad_game', 'bad_loader', 'bad_function']:
        text = f'''#pragma once
#include <cstddef>
#include <cstdint>
namespace ffr::profile {{
inline constexpr char game_sha256[] = "{'0'*64 if name=='bad_game' else game_hash}";
inline constexpr char loader_sha256[] = "{'0'*64 if name=='bad_loader' else loader_hash}";
inline constexpr char prologue_sha256[] = "{code_hash}";
inline constexpr std::uintptr_t entry_rva = {entry};
inline constexpr std::size_t prologue_size = 64;
inline constexpr std::uintptr_t constructor_rva = {loader_pe.exports[ctor]};
inline constexpr std::uintptr_t destructor_rva = {loader_pe.exports[dtor]};
inline constexpr std::size_t host_storage_size = 4096;
inline constexpr char constructor_name[] = "{ctor}";
inline constexpr char destructor_name[] = "{dtor}";
'''
        for function,rva in extra_functions.items():
            start=game_pe.offset(rva)
            digest=hashlib.sha256(game_pe.raw[start:start+64]).hexdigest()
            if name=='bad_function' and function=='save_memory': digest='0'*64
            text += f'inline constexpr std::uintptr_t {function}_rva = {rva};\ninline constexpr char {function}_sha256[] = "{digest}";\n'
        text += '}\n'
        (output/(name+'_profile.hpp')).write_text(text, encoding='utf8')


if __name__ == '__main__': main()
