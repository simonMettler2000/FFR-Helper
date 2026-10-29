#pragma once
#include <cstddef>
#include <cstdint>
namespace ffr::task_abort_profile {
// Exact build24250053. The live ordinary input step2 has no update callback.
// These constants alone do not implement or authorize a task handoff.
inline constexpr std::uintptr_t end_command_rva=0x516e020;
inline constexpr std::uintptr_t finish_check_rva=0x4faace0;
inline constexpr std::uintptr_t task_class_cache_rva=0x9f52608;
inline constexpr std::uintptr_t talk_class_cache_rva=0x9f52830;
inline constexpr std::uintptr_t root_vm_class_cache_rva=0x9f1a008;
inline constexpr std::uintptr_t command_vm_class_cache_rva=0x9f54760;
inline constexpr std::uintptr_t task_vtable_rva=0x8650248;
inline constexpr std::uintptr_t step_vtable_rva=0x885ae68;
inline constexpr std::uintptr_t command_vm_vtable_rva=0x8892658;
inline constexpr std::uintptr_t result_skip_vtable_rva=0x8650b08;
inline constexpr std::uintptr_t step_begin_rva=0x4faa140;
inline constexpr std::uintptr_t empty_update_rva=0x44ac040;
struct CodeRegion {std::uintptr_t rva;std::size_t size;const char* sha256;};
inline constexpr CodeRegion regions[]{
    {0x4faace0,889,"93a5ef0ace960ac5a1e92d99747e34713fac2f4fff1a597c0d36de50b9d1eba7"}, // finishCheck
    {0x4fcaf60,81,"ae74ea87462bb0b1a1d406e89a05c2237aecaa51a82038c8569f55b71a794de1"}, // turnStep0Update
    {0x4fcafb1,50,"367dcd1929dcd090a1344230ec26444b09154acc03b8837fd260f962002d6fab"}, // turnStep0Update
    {0x4fcafe3,1138,"6b505664c0134ffa358226ba6afb024dadeede944b6ee35f9fd92e4a1c716253"}, // turnStep0Update
    {0x4fcb455,21,"b34d330b071ddb9cc284f2ec153eddc55fd8cd117f0081ae37d22a148cb0e953"}, // turnStep0Update
    {0x4fcb46a,24,"c0acf49aea8ee5355db8d472b67943de775411b7ffed0458944eabee8b29c092"}, // turnStep0Update
    {0x4fcb482,9,"b2076bd3509e7d3b45543d82b9aca62ba4140b2cba410e9e7a2692c453ede04c"}, // turnStep0Update
    {0x516e020,242,"2514d1bb97a302522abf99896ad4fa661a7b081efb515314548cb2d2b232bb10"}, // endCommand
    {0x516a3a0,200,"be9ee908a776a5b8629b6e5a69c022e34af70641b7d3014d173c438ff6638cb5"}, // endCommandClear
    {0x4f3bd80,113,"5945fdd204cfb7959173542e3545f4c077305f72939c401b22bfad76ea3b2242"}, // taskGetter
    {0x4ea6fa0,174,"caa2f6855880e8b3eb931ae6ececccced983e1c2be7b48ef6850dad98b11c3bf"}, // taskClass
    {0x523aaf0,20,"ab8b5d99a350f7ce9d3276abc57a10a0d6fc19ed187fd1ed2d86b2af292d6504"}, // rootVmGetter
    {0x523ab04,265,"3ab1252d8366d6d79830c4ffafdc5f73e90e3e4794314ef398cb3d9c0dfadea5"}, // rootVmGetter
    {0x523ac0d,14,"aa11ca541a6a5c9c6172cddcf8a7677bb916e070192c2ebe292f4eec59d337aa"}, // rootVmGetter
    {0x5240200,43,"6e3fc19b0a8337eec34819707e5c95b9fd72f38da67ee4875dcae412ebdf0de0"}, // rootVmContainer
    {0x4562860,174,"f3da56ce6908cc6116bfda5d35f3e95a0b126d46a33f077bc19298d53dd6522d"}, // rootVmClass
    {0x456b280,156,"dec32ea66e7f6d35d6a414615f234a4f33f82272b92d6e427d103ba69ba167bd"}, // registryFind
    {0x456f1c0,320,"defad6af3429539fd59057630526a14f67c2080491a93396778b1865a96caafd"}, // registryMatch
    {0x4ec90d0,174,"5de32bb29d609deca14dcb9ba5f0e4e458592596d153ad75f2ebb0ec1e4b3f02"}, // commandVmClass
    {0x4fadb00,397,"c4ed5b4a2b10567ae0708888b55e05d27e1350eeaf46456e857c0e83d5fbc9b8"}, // turnStepFactory
    {0x4f9fa90,140,"1598944cf161399c7ff081e42ed62a012ef368952228670034e10da0546ec0b0"}, // resultSkipFactory
    {0x4d9dac0,41,"de87482623059b125d9b92c44d3f692f30f4d7f3380f84fa11a47c1e2c0493a6"}, // resultSkipConstructor
    {0x4d9aa50,174,"2164fa31dc63ad096bd3e394b265455bf488472c27c9e317ab503b8a21a2e4c1"}, // resultSkipClass
    {0x51555e0,14,"6b24da56152bd46d3b784e61d0e15a8099562693543a767749b2c65aa0e523fb"}, // taskSetStep
    {0x51555ee,222,"33ff0e49940a030e116bbb7687ff6e9e06e050ce07c04317159ff0e389b196e9"}, // taskSetStep
    {0x51556cc,65,"9732bdf0dfa1cb3dc7354dd94a188ab733ccdbee854f171b158ee1635a0e89a6"}, // taskSetStep
    {0x51558e0,95,"f559aa8c7b1af09b227f72ba9d71068bcbc980d807662b63f17b822e51d7656e"}, // taskSetTask
    {0x51428a0,29,"f62bea103f70507f62c854e87f8b0901594a59b46f52d80445dd3fc6795e4ffe"}, // taskEnd
    {0x51428bd,59,"2311e4b0dcdfb5e27eb9499b49635254b3788ecbc1889a9b49459deebd858675"}, // taskEnd
    {0x51428f8,5,"cf50cdf82c49d63c3e44bf127877009405eb024566c569bc6192d840e03613a1"}, // taskEnd
    {0x51428fd,5,"3297a0e4175e5d032a2be5212c1a1980cd3df21251db492413a47fcb4b6187ac"}, // taskEnd
    {0x4faa140,8,"f749dfbee9c50dc74af29e2bd35586a7632385f6e45da854b6c37ab84d1c4828"}, // inputStepBegin
    {0x44ac040,3,"b6cbececf1b07e74f37d57710c68c31f0b92cb08d51093252b321b07542e177b"}, // emptyUpdate
    {0x514cfb0,11,"d4295019870086346bee9e44f8062e288c8d602928550fb6ff499890b7ff7f73"}, // taskBegin
    {0x4fade70,5,"9631cc5048196f39829c1d4a34b05b5ba8b2f9ed1f142f815667b22f59f6a551"}, // resultSkipStepOpaqueEntry
    {0x4ea8a20,174,"29d04e194d5d95a6dc643ef3bcd0d7464c63779c82f039009d84027be62dfc08"}, // talkClass
    {0x51ddd00,40,"33b66ca7f3c39a01d6b02576cf6db7aa3baf8e826a393b0338e66028c832a495"}, // native talk busy predicate
    {0x4fc58d0,17,"4f7644f73f9ee3f938cd7ec069b8d76b2f6b388ee6c06f5ea4beb6356e26431b"}, // turnPrechecks
    {0x4fc58e1,266,"e2bfc67cf4fd98310c6025d8a074a376e5deb7e31cb7fae042a83e1d2d30e4cd"}, // turnPrechecks
    {0x4fc59eb,210,"9067b6156e34251ffc9f2f5ac03f90ac7ee6c583e99e553e03d3e71ba34acc14"}, // turnPrechecks
    {0x4fc5abd,52,"dfbb1b318ca4d4496759da56f5225c403182f4bb1105bc15b0bc58e992603b8e"}, // turnPrechecks
    {0x4fc5af1,38,"c808eb989896651488848dedfc01418053feb664b851ce672a18b40dd3f5a7bc"}, // turnPrechecks
    {0x4f40b90,283,"e6137ff3dbb8ef47e17d73b4e4863144369472755359cb2c1e15901c0e930eac"}, // turnEventRequests
    {0x4f59c00,41,"6eba9ab441a4e58b2b28abd5274fc14a56237b206a24c58c7955cd386b9f232c"}, // pendingRequests
};
} // namespace ffr::task_abort_profile
