#define CATCH_CONFIG_MAIN
#include "catch/catch.hpp"
#include "text.hpp"

TEST_CASE("UTF-8 enforcement", "[utf8]") {
	REQUIRE(check_utf8("") == std::string(""));
	REQUIRE(check_utf8("hello world") == std::string(""));
	REQUIRE(check_utf8("Καλημέρα κόσμε") == std::string(""));
	REQUIRE(check_utf8("こんにちは 世界") == std::string(""));
	REQUIRE(check_utf8("👋🌏") == std::string(""));
	REQUIRE(check_utf8("Hola m\xF3n") == std::string("\"Hola m\xF3n\" is not valid UTF-8 (0xF3 0x6E)"));
}
