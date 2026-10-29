#pragma once
#include "native_encounter_binding.hpp"
// The including translation unit supplies its capture profile first. Keeping
// that identity explicit also avoids mixing production and mock namespaces.
namespace ffr {
// Exact consuming map-entry/copy/destructor/allocator chain. This does not
// establish a safe field transaction or enable a game command by itself.
inline NativeEncounterProfile supported_native_encounter_profile() {
    return {profile::game_sha256,
        {84200624,418,"a87de772f8200020137266e943da1969cba876928a6c60cd3e2d6c3195d039d4"},
        {19138240,62,"570ca331245524da883920d82b124c2ce7d14fd61a45bc9c6263947c584b27dd"},
        {19083760,52,"3b82e827419481846011f17822f1e1ebbb0fb15db9067b1ea1fd418ae79dd61f"},
        {
            {81301552,115,"664f6f0b76e9090f7db8d2ac70821cedd8913d32b5e30ecb52a44f763ce38657"}, // inputDestructor
            {79921824,52,"cc91efb806a420ef6ebf1a1263d5494e4b9d4cc6a25936b7ca40bb8c03ec0d46"}, // nestedDestructor
            {83236416,6,"5de93af64e928c1c087b633b19fae91d96e3ee76cf7be0f5097a1778dc740661"}, // battleCopy
            {83236422,99,"56912d495eaf7950acbf654db3a3479d847ab779bf2316aa758a53a8ca652dfe"}, // battleCopy
            {83236521,89,"494f85cdc354708d35baf0f97866bc1542ed85f8ae5408d8cdcd7023ca599bb4"}, // battleCopy
            {83236610,164,"109ef60ab66ca44c5ef6541308c1c6a114ea404c7b600a4ecb478334b2eeef4b"}, // battleCopy
            {83236774,27,"d4bd13aefb0919a35f481ee79fe2a5341835a2c89d785046cfc99ff16dd259b6"}, // battleCopy
            {20990672,22,"49f4030f1f84228fcd09c68bdd3438d6efa6ba4d7b2a94d58b4bc0fa6d598549"}, // intArrayCopy
            {20990694,50,"cdf8a87af419251e1cbd869beaf83f02228eee58d584ac5e01e62ed6c791acc9"}, // intArrayCopy
            {20990744,61,"5ad1f64e6e800ab4c70977af77cc699fddbfba9977a86b1555f967ca1b3617e3"}, // intArrayCopy
            {20990805,9,"2b2fef5ae31f8d8eb8303f60e2ce735e281afe148a327a028b151d448d61dd76"}, // intArrayCopy
            {80955424,17,"1758341ab7a2245e1d828dd4ba0f4f5d2ce452bad869fb6924046e4a82cc4ec4"}, // nestedCopy
            {80955441,141,"fc68fca4bb6e4ecdcaac089b3756fb6d7117e4843acd259ea59e8f2be73a00d2"}, // nestedCopy
            {80955582,1,"ae3f4619b0413d70d3004b9131c3752153074e45725be13b9a148978895e359e"}, // nestedCopy
            {18643472,22,"d26934dd2dc9ef6b4842600eb790e76d460993d54b98e3caf1aeb7d17a69cb3c"}, // bgmCopy
            {18643494,50,"cdf8a87af419251e1cbd869beaf83f02228eee58d584ac5e01e62ed6c791acc9"}, // bgmCopy
            {18643544,60,"24dccea3bff5539ecd7f0de54b26328f0d804c41d7f538c931abec52eb4fcb54"}, // bgmCopy
            {18643604,9,"2b2fef5ae31f8d8eb8303f60e2ce735e281afe148a327a028b151d448d61dd76"}, // bgmCopy
            {83824624,5,"76d0911b237565940601fe406be619416f05af51ef992d95905668ed4a8befd3"}, // entryForwardJump
            {463312912,94,"8cca9d9647f142b45c380693e906869d4cc6401b6393243ad9435bc3550f2947"}, // entryForwardTarget
        },true};
}
} // namespace ffr
