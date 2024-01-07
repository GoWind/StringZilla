#include <algorithm> // `std::transform`
#include <cassert>   // assertions
#include <cstdio>    // `std::printf`
#include <cstring>   // `std::memcpy`
#include <iterator>  // `std::distance`
#include <random>    // `std::random_device`
#include <vector>    // `std::vector`

#define SZ_USE_X86_AVX2 0
#define SZ_USE_X86_AVX512 1
#define SZ_USE_ARM_NEON 0
#define SZ_USE_ARM_SVE 0

#include <string>                      // Baseline
#include <string_view>                 // Baseline
#include <stringzilla/stringzilla.hpp> // Contender

namespace sz = ashvardanian::stringzilla;
using sz::literals::operator""_sz;

/**
 *  Several string processing operations rely on computing logarithms and powers of
 */
static void test_arithmetical_utilities() {

    assert(sz_u64_clz(0x0000000000000001ull) == 63);
    assert(sz_u64_clz(0x0000000000000002ull) == 62);
    assert(sz_u64_clz(0x0000000000000003ull) == 62);
    assert(sz_u64_clz(0x0000000000000004ull) == 61);
    assert(sz_u64_clz(0x0000000000000007ull) == 61);
    assert(sz_u64_clz(0x8000000000000001ull) == 0);
    assert(sz_u64_clz(0xffffffffffffffffull) == 0);
    assert(sz_u64_clz(0x4000000000000000ull) == 1);

    assert(sz_size_log2i_nonzero(1) == 0);
    assert(sz_size_log2i_nonzero(2) == 1);
    assert(sz_size_log2i_nonzero(3) == 1);

    assert(sz_size_log2i_nonzero(4) == 2);
    assert(sz_size_log2i_nonzero(5) == 2);
    assert(sz_size_log2i_nonzero(7) == 2);

    assert(sz_size_log2i_nonzero(8) == 3);
    assert(sz_size_log2i_nonzero(9) == 3);

    assert(sz_size_bit_ceil(0) == 0);
    assert(sz_size_bit_ceil(1) == 1);

    assert(sz_size_bit_ceil(2) == 2);
    assert(sz_size_bit_ceil(3) == 4);
    assert(sz_size_bit_ceil(4) == 4);

    assert(sz_size_bit_ceil(77) == 128);
    assert(sz_size_bit_ceil(127) == 128);
    assert(sz_size_bit_ceil(128) == 128);

    assert(sz_size_bit_ceil(uint64_t(1e6)) == (1ull << 20));
    assert(sz_size_bit_ceil(uint64_t(2e6)) == (1ull << 21));
    assert(sz_size_bit_ceil(uint64_t(4e6)) == (1ull << 22));
    assert(sz_size_bit_ceil(uint64_t(8e6)) == (1ull << 23));

    assert(sz_size_bit_ceil(uint64_t(1.6e7)) == (1ull << 24));
    assert(sz_size_bit_ceil(uint64_t(3.2e7)) == (1ull << 25));
    assert(sz_size_bit_ceil(uint64_t(6.4e7)) == (1ull << 26));

    assert(sz_size_bit_ceil(uint64_t(1.28e8)) == (1ull << 27));
    assert(sz_size_bit_ceil(uint64_t(2.56e8)) == (1ull << 28));
    assert(sz_size_bit_ceil(uint64_t(5.12e8)) == (1ull << 29));

    assert(sz_size_bit_ceil(uint64_t(1e9)) == (1ull << 30));
    assert(sz_size_bit_ceil(uint64_t(2e9)) == (1ull << 31));
    assert(sz_size_bit_ceil(uint64_t(4e9)) == (1ull << 32));
    assert(sz_size_bit_ceil(uint64_t(8e9)) == (1ull << 33));

    assert(sz_size_bit_ceil(uint64_t(1.6e10)) == (1ull << 34));

    assert(sz_size_bit_ceil((1ull << 62)) == (1ull << 62));
    assert(sz_size_bit_ceil((1ull << 62) + 1) == (1ull << 63));
    assert(sz_size_bit_ceil((1ull << 63)) == (1ull << 63));
}

static void test_constructors() {
    std::string alphabet {sz::ascii_printables};
    std::vector<sz::string> strings;
    for (std::size_t alphabet_slice = 0; alphabet_slice != alphabet.size(); ++alphabet_slice)
        strings.push_back(alphabet.substr(0, alphabet_slice));
    std::vector<sz::string> copies {strings};
    assert(copies.size() == strings.size());
    for (size_t i = 0; i < copies.size(); i++) {
        assert(copies[i].size() == strings[i].size());
        assert(copies[i] == strings[i]);
        for (size_t j = 0; j < strings[i].size(); j++) { assert(copies[i][j] == strings[i][j]); }
    }
    std::vector<sz::string> assignments = strings;
    for (size_t i = 0; i < assignments.size(); i++) {
        assert(assignments[i].size() == strings[i].size());
        assert(assignments[i] == strings[i]);
        for (size_t j = 0; j < strings[i].size(); j++) { assert(assignments[i][j] == strings[i][j]); }
    }
    assert(std::equal(strings.begin(), strings.end(), copies.begin()));
    assert(std::equal(strings.begin(), strings.end(), assignments.begin()));
}

static void test_updates() {
    // Compare STL and StringZilla strings append functionality.
    char const alphabet_chars[] = "abcdefghijklmnopqrstuvwxyz";
    std::string stl_string;
    sz::string sz_string;
    for (std::size_t length = 1; length != 200; ++length) {
        char c = alphabet_chars[std::rand() % 26];
        stl_string.push_back(c);
        sz_string.push_back(c);
        assert(sz::string_view(stl_string) == sz::string_view(sz_string));
    }

    // Compare STL and StringZilla strings erase functionality.
    while (stl_string.length()) {
        std::size_t offset_to_erase = std::rand() % stl_string.length();
        std::size_t chars_to_erase = std::rand() % (stl_string.length() - offset_to_erase) + 1;
        stl_string.erase(offset_to_erase, chars_to_erase);
        sz_string.erase(offset_to_erase, chars_to_erase);
        assert(sz::string_view(stl_string) == sz::string_view(sz_string));
    }
}

static void test_comparisons() {
    // Comparing relative order of the strings
    assert("a"_sz.compare("a") == 0);
    assert("a"_sz.compare("ab") == -1);
    assert("ab"_sz.compare("a") == 1);
    assert("a"_sz.compare("a\0"_sz) == -1);
    assert("a\0"_sz.compare("a") == 1);
    assert("a\0"_sz.compare("a\0"_sz) == 0);
    assert("a"_sz == "a"_sz);
    assert("a"_sz != "a\0"_sz);
    assert("a\0"_sz == "a\0"_sz);
}

static void test_search() {

    // Searching for a set of characters
    assert(sz::string_view("a").find_first_of("az") == 0);
    assert(sz::string_view("a").find_last_of("az") == 0);
    assert(sz::string_view("a").find_first_of("xz") == sz::string_view::npos);
    assert(sz::string_view("a").find_last_of("xz") == sz::string_view::npos);

    assert(sz::string_view("a").find_first_not_of("xz") == 0);
    assert(sz::string_view("a").find_last_not_of("xz") == 0);
    assert(sz::string_view("a").find_first_not_of("az") == sz::string_view::npos);
    assert(sz::string_view("a").find_last_not_of("az") == sz::string_view::npos);

    assert(sz::string_view("aXbYaXbY").find_first_of("XY") == 1);
    assert(sz::string_view("axbYaxbY").find_first_of("Y") == 3);
    assert(sz::string_view("YbXaYbXa").find_last_of("XY") == 6);
    assert(sz::string_view("YbxaYbxa").find_last_of("Y") == 4);
    assert(sz::string_view(sz::base64).find_first_of("_") == sz::string_view::npos);
    assert(sz::string_view(sz::base64).find_first_of("+") == 62);
    assert(sz::string_view(sz::ascii_printables).find_first_of("~") != sz::string_view::npos);

    // Check more advanced composite operations:
    assert("abbccc"_sz.partition("bb").before.size() == 1);
    assert("abbccc"_sz.partition("bb").match.size() == 2);
    assert("abbccc"_sz.partition("bb").after.size() == 3);
    assert("abbccc"_sz.partition("bb").before == "a");
    assert("abbccc"_sz.partition("bb").match == "bb");
    assert("abbccc"_sz.partition("bb").after == "ccc");

    // Check ranges of search matches
    assert(""_sz.find_all(".").size() == 0);
    assert("a.b.c.d"_sz.find_all(".").size() == 3);
    assert("a.,b.,c.,d"_sz.find_all(".,").size() == 3);
    assert("a.,b.,c.,d"_sz.rfind_all(".,").size() == 3);
    assert("a.b,c.d"_sz.find_all(sz::character_set(".,")).size() == 3);
    assert("a...b...c"_sz.rfind_all("..", true).size() == 4);

    auto finds = "a.b.c"_sz.find_all(sz::character_set("abcd")).template to<std::vector<std::string>>();
    assert(finds.size() == 3);
    assert(finds[0] == "a");

    auto rfinds = "a.b.c"_sz.rfind_all(sz::character_set("abcd")).template to<std::vector<std::string>>();
    assert(rfinds.size() == 3);
    assert(rfinds[0] == "c");

    auto splits = ".a..c."_sz.split(sz::character_set(".")).template to<std::vector<std::string>>();
    assert(splits.size() == 5);
    assert(splits[0] == "");
    assert(splits[1] == "a");
    assert(splits[4] == "");

    assert(""_sz.split(".").size() == 1);
    assert(""_sz.rsplit(".").size() == 1);
    assert("a.b.c.d"_sz.split(".").size() == 4);
    assert("a.b.c.d"_sz.rsplit(".").size() == 4);
    assert("a.b.,c,d"_sz.split(".,").size() == 2);
    assert("a.b,c.d"_sz.split(sz::character_set(".,")).size() == 4);

    auto rsplits = ".a..c."_sz.rsplit(sz::character_set(".")).template to<std::vector<std::string>>();
    assert(rsplits.size() == 5);
    assert(rsplits[0] == "");
    assert(rsplits[1] == "c");
    assert(rsplits[4] == "");
}

/**
 *  Evaluates the correctness of a "matcher", searching for all the occurences of the `needle_stl`
 *  in a haystack formed of `haystack_pattern` repeated from one to `max_repeats` times.
 *
 *  @param misalignment The number of bytes to misalign the haystack within the cacheline.
 */
template <typename stl_matcher_, typename sz_matcher_>
void test_search_with_misaligned_repetitions(std::string_view haystack_pattern, std::string_view needle_stl,
                                             std::size_t misalignment) {
    constexpr std::size_t max_repeats = 128;
    alignas(64) char haystack[misalignment + max_repeats * haystack_pattern.size()];
    std::vector<std::size_t> offsets_stl;
    std::vector<std::size_t> offsets_sz;

    for (std::size_t repeats = 0; repeats != 128; ++repeats) {
        std::size_t haystack_length = (repeats + 1) * haystack_pattern.size();
        std::memcpy(haystack + misalignment + repeats * haystack_pattern.size(), haystack_pattern.data(),
                    haystack_pattern.size());

        // Convert to string views
        auto haystack_stl = std::string_view(haystack + misalignment, haystack_length);
        auto haystack_sz = sz::string_view(haystack + misalignment, haystack_length);
        auto needle_sz = sz::string_view(needle_stl.data(), needle_stl.size());

        // Wrap into ranges
        auto matches_stl = stl_matcher_(haystack_stl, {needle_stl});
        auto matches_sz = sz_matcher_(haystack_sz, {needle_sz});
        auto begin_stl = matches_stl.begin();
        auto begin_sz = matches_sz.begin();
        auto end_stl = matches_stl.end();
        auto end_sz = matches_sz.end();
        auto count_stl = std::distance(begin_stl, end_stl);
        auto count_sz = std::distance(begin_sz, end_sz);

        // To simplify debugging, let's first export all the match offsets, and only then compare them
        std::transform(begin_stl, end_stl, std::back_inserter(offsets_stl),
                       [&](auto const &match) { return match.data() - haystack_stl.data(); });
        std::transform(begin_sz, end_sz, std::back_inserter(offsets_sz),
                       [&](auto const &match) { return match.data() - haystack_sz.data(); });
        auto print_all_matches = [&]() {
            std::printf("Breakdown of found matches:\n");
            std::printf("- STL (%zu): ", offsets_stl.size());
            for (auto offset : offsets_stl) std::printf("%zu ", offset);
            std::printf("\n");
            std::printf("- StringZilla (%zu): ", offsets_sz.size());
            for (auto offset : offsets_sz) std::printf("%zu ", offset);
            std::printf("\n");
        };

        // Compare results
        for (std::size_t match_idx = 0; begin_stl != end_stl && begin_sz != end_sz;
             ++begin_stl, ++begin_sz, ++match_idx) {
            auto match_stl = *begin_stl;
            auto match_sz = *begin_sz;
            if (match_stl.data() != match_sz.data()) {
                std::printf("Mismatch at index #%zu: %zu != %zu\n", match_idx, match_stl.data() - haystack_stl.data(),
                            match_sz.data() - haystack_sz.data());
                print_all_matches();
                assert(false);
            }
        }

        // If one range is not finished, assert failure
        if (count_stl != count_sz) {
            print_all_matches();
            assert(false);
        }
        assert(begin_stl == end_stl && begin_sz == end_sz);

        offsets_stl.clear();
        offsets_sz.clear();
    }
}

/**
 *  Evaluates the correctness of a "matcher", searching for all the occurences of the `needle_stl`,
 *  as a substring, as a set of allowed characters, or as a set of disallowed characters, in a haystack.
 */
void test_search_with_misaligned_repetitions(std::string_view haystack_pattern, std::string_view needle_stl,
                                             std::size_t misalignment) {

    test_search_with_misaligned_repetitions<                   //
        sz::range_matches<std::string_view, sz::matcher_find>, //
        sz::range_matches<sz::string_view, sz::matcher_find>>( //
        haystack_pattern, needle_stl, misalignment);

    test_search_with_misaligned_repetitions<                     //
        sz::range_rmatches<std::string_view, sz::matcher_rfind>, //
        sz::range_rmatches<sz::string_view, sz::matcher_rfind>>( //
        haystack_pattern, needle_stl, misalignment);

    test_search_with_misaligned_repetitions<                            //
        sz::range_matches<std::string_view, sz::matcher_find_first_of>, //
        sz::range_matches<sz::string_view, sz::matcher_find_first_of>>( //
        haystack_pattern, needle_stl, misalignment);

    test_search_with_misaligned_repetitions<                            //
        sz::range_rmatches<std::string_view, sz::matcher_find_last_of>, //
        sz::range_rmatches<sz::string_view, sz::matcher_find_last_of>>( //
        haystack_pattern, needle_stl, misalignment);

    test_search_with_misaligned_repetitions<                                //
        sz::range_matches<std::string_view, sz::matcher_find_first_not_of>, //
        sz::range_matches<sz::string_view, sz::matcher_find_first_not_of>>( //
        haystack_pattern, needle_stl, misalignment);

    test_search_with_misaligned_repetitions<                                //
        sz::range_rmatches<std::string_view, sz::matcher_find_last_not_of>, //
        sz::range_rmatches<sz::string_view, sz::matcher_find_last_not_of>>( //
        haystack_pattern, needle_stl, misalignment);
}

void test_search_with_misaligned_repetitions(std::string_view haystack_pattern, std::string_view needle_stl) {
    test_search_with_misaligned_repetitions(haystack_pattern, needle_stl, 0);
    test_search_with_misaligned_repetitions(haystack_pattern, needle_stl, 1);
    test_search_with_misaligned_repetitions(haystack_pattern, needle_stl, 2);
    test_search_with_misaligned_repetitions(haystack_pattern, needle_stl, 3);
    test_search_with_misaligned_repetitions(haystack_pattern, needle_stl, 63);
    test_search_with_misaligned_repetitions(haystack_pattern, needle_stl, 24);
    test_search_with_misaligned_repetitions(haystack_pattern, needle_stl, 33);
}

void test_search_with_misaligned_repetitions() {
    // When haystack is only formed of needles:
    test_search_with_misaligned_repetitions("a", "a");
    test_search_with_misaligned_repetitions("ab", "ab");
    test_search_with_misaligned_repetitions("abc", "abc");
    test_search_with_misaligned_repetitions("abcd", "abcd");
    test_search_with_misaligned_repetitions(sz::ascii_lowercase, sz::ascii_lowercase);
    test_search_with_misaligned_repetitions(sz::ascii_printables, sz::ascii_printables);

    // When we are dealing with NULL characters inside the string
    test_search_with_misaligned_repetitions("\0", "\0");
    test_search_with_misaligned_repetitions("a\0", "a\0");
    test_search_with_misaligned_repetitions("ab\0", "ab");
    test_search_with_misaligned_repetitions("ab\0", "ab\0");
    test_search_with_misaligned_repetitions("abc\0", "abc");
    test_search_with_misaligned_repetitions("abc\0", "abc\0");
    test_search_with_misaligned_repetitions("abcd\0", "abcd");

    // When haystack is formed of equidistant needles:
    test_search_with_misaligned_repetitions("ab", "a");
    test_search_with_misaligned_repetitions("abc", "a");
    test_search_with_misaligned_repetitions("abcd", "a");

    // When matches occur in between pattern words:
    test_search_with_misaligned_repetitions("ab", "ba");
    test_search_with_misaligned_repetitions("abc", "ca");
    test_search_with_misaligned_repetitions("abcd", "da");
}

std::size_t levenshtein_baseline(std::string_view s1, std::string_view s2) {
    std::size_t len1 = s1.size();
    std::size_t len2 = s2.size();

    std::vector<std::vector<std::size_t>> dp(len1 + 1, std::vector<std::size_t>(len2 + 1));

    // Initialize the borders of the matrix.
    for (std::size_t i = 0; i <= len1; ++i) dp[i][0] = i;
    for (std::size_t j = 0; j <= len2; ++j) dp[0][j] = j;

    for (std::size_t i = 1; i <= len1; ++i) {
        for (std::size_t j = 1; j <= len2; ++j) {
            std::size_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            // dp[i][j] is the minimum of deletion, insertion, or substitution
            dp[i][j] = std::min({
                dp[i - 1][j] + 1,       // Deletion
                dp[i][j - 1] + 1,       // Insertion
                dp[i - 1][j - 1] + cost // Substitution
            });
        }
    }

    return dp[len1][len2];
}

static void test_levenshtein_distances() {
    struct {
        char const *left;
        char const *right;
        std::size_t distance;
    } explicit_cases[] = {
        {"", "", 0},
        {"", "abc", 3},
        {"abc", "", 3},
        {"abc", "ac", 1},                   // one deletion
        {"abc", "a_bc", 1},                 // one insertion
        {"abc", "adc", 1},                  // one substitution
        {"ggbuzgjux{}l", "gbuzgjux{}l", 1}, // one insertion (prepended
    };

    auto print_failure = [&](sz::string const &l, sz::string const &r, std::size_t expected, std::size_t received) {
        char const *ellipsis = l.length() > 22 || r.length() > 22 ? "..." : "";
        std::printf("Levenshtein distance error: distance(\"%.22s%s\", \"%.22s%s\"); got %zd, expected %zd\n", //
                    l.c_str(), ellipsis, r.c_str(), ellipsis, received, expected);
    };

    auto test_distance = [&](sz::string const &l, sz::string const &r, std::size_t expected) {
        auto received = l.edit_distance(r);
        if (received != expected) print_failure(l, r, expected, received);
        // The distance relation commutes
        received = r.edit_distance(l);
        if (received != expected) print_failure(r, l, expected, received);
    };

    for (auto explicit_case : explicit_cases)
        test_distance(sz::string(explicit_case.left), sz::string(explicit_case.right), explicit_case.distance);

    // Randomized tests
    // TODO: Add bounded distance tests
    struct {
        std::size_t length_upper_bound;
        std::size_t iterations;
    } fuzzy_cases[] = {
        {10, 1000},
        {100, 100},
        {1000, 10},
    };
    std::random_device random_device;
    std::mt19937 generator(random_device());
    sz::string first, second;
    for (auto fuzzy_case : fuzzy_cases) {
        char alphabet[2] = {'a', 'b'};
        std::uniform_int_distribution<std::size_t> length_distribution(0, fuzzy_case.length_upper_bound);
        for (std::size_t i = 0; i != fuzzy_case.iterations; ++i) {
            std::size_t first_length = length_distribution(generator);
            std::size_t second_length = length_distribution(generator);
            std::generate_n(std::back_inserter(first), first_length, [&]() { return alphabet[generator() % 2]; });
            std::generate_n(std::back_inserter(second), second_length, [&]() { return alphabet[generator() % 2]; });
            test_distance(first, second, levenshtein_baseline(first, second));
            first.clear();
            second.clear();
        }
    }
}

int main(int argc, char const **argv) {

    // Let's greet the user nicely
    static const char *USER_NAME =
#define str(s) #s
#define xstr(s) str(s)
        xstr(DEV_USER_NAME);
    std::printf("Hi " xstr(DEV_USER_NAME) "! You look nice today!\n");
#undef str
#undef xstr

    // Basic utilities
    test_arithmetical_utilities();

    // The string class implementation
    test_constructors();
    test_updates();

    // Advanced search operations
    test_comparisons();
    test_search();
    test_search_with_misaligned_repetitions();
    test_levenshtein_distances();

    return 0;
}
