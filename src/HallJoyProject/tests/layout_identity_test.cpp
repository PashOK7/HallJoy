#include "generated/layout_pipeline/identities.h"
#include <cassert>
#include <string_view>

int main()
{
    using namespace halljoy::layout_identity;
    assert(Match(0)==nullptr && Match(~std::uint64_t{0})==nullptr);
    assert(Token("aula-w669","unknown")==0);
    assert(Token("aula-rm6x21","SI2825HEARGB")==0);
    assert(Token("aula-w669","0A021902")==0);
    assert(Token("aula-w669","7272BRHEXYXK673JCARGB")==0);
    assert(Token("aula-w669","7272USHEXYXK673JCARGB")!=0);
    assert(Token("aula-w669","7272UKHEXYXBJCARGB")!=0);
    assert(Token("aula-w669","7272UKHEXYXBJCARGB")!=Token("aula-w669","7272USHEXYXK673JCARGB"));
    assert(Token("aula-w669","SI2825HEARGB")==Token("aula-w669","SI2825KZHEARGB"));
    assert(Token("aula-w669","SI2825HEARGB")!=Token("aula-w669","SI2828HEARGB"));
    for (const auto& e : entries) {
        assert(e.token!=0);
        assert(Token(e.protocol,e.product)==e.token);
        assert(std::wstring_view(Match(e.token))==e.preset);
    }
}
