#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <set>

namespace {
std::atomic<int> alive{0};
std::atomic<int> errors{0};
std::atomic<bool> initialized{true};
std::set<void*> roots;
}
namespace RC {
class __declspec(dllexport) CppUserModBase {
    std::vector<std::shared_ptr<void>> tabs;
    std::wstring name, version, description, authors, sdk;
public:
    CppUserModBase();
    virtual ~CppUserModBase();
    virtual void on_update() {}
};
static_assert(sizeof(CppUserModBase) == 192);
CppUserModBase::CppUserModBase() : name(L"Native fixture"), version(L"test"), sdk(L"test") { ++alive; }
CppUserModBase::~CppUserModBase() { if (name != L"Native fixture") ++errors; --alive; }
}
extern "C" __declspec(dllexport) int ffr_test_hosts_alive() { return alive.load(); }
extern "C" __declspec(dllexport) int ffr_test_host_errors() { return errors.load(); }
extern "C" __declspec(dllexport) const void* ffr_test_loader_ready() {return &initialized;}
extern "C" __declspec(dllexport) void ffr_test_loader_set_ready(bool value) {initialized.store(value);}
extern "C" __declspec(dllexport) bool ffr_test_object_rooted(void* object) {return roots.count(object)!=0;}
extern "C" __declspec(dllexport) void ffr_test_object_add_root(void* object) {if(!roots.insert(object).second)++errors;}
extern "C" __declspec(dllexport) void ffr_test_object_remove_root(void* object) {if(roots.erase(object)!=1)++errors;}
extern "C" __declspec(dllexport) unsigned ffr_test_root_count() {return static_cast<unsigned>(roots.size());}
