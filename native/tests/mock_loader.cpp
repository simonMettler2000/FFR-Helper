#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace {
std::atomic<int> alive{0};
std::atomic<int> errors{0};
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
