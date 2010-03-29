#include "TestSupport.h"
#include "Utils.h"
#include "Utils/MemZeroGuard.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

using namespace Passenger;
using namespace std;

namespace tut {
	struct UtilsTest {
		vector<string> output;
		string oldPath;
		TempDir tempDir;
		
		UtilsTest(): tempDir("tmp.dir") {
			oldPath = getenv("PATH");
			unsetenv("PASSENGER_TEMP_DIR");
		}
		
		~UtilsTest() {
			setenv("PATH", oldPath.c_str(), 1);
			unsetenv("PASSENGER_TEMP_DIR");
		}
		
		void testMakeDirTreeMode(const char *name, const char *mode, mode_t expected) {
			TempDir td("tmp.dir2");
			struct stat buf;
			mode_t allModes = S_IRWXU | S_ISUID | S_IRWXG | S_ISGID | S_IRWXO;
			
			makeDirTree("tmp.dir2/foo", mode);
			stat("tmp.dir2/foo", &buf);
			ensure_equals(name, buf.st_mode & allModes, expected);
		}
	};
	
	DEFINE_TEST_GROUP(UtilsTest);

	/***** Test split() *****/

	TEST_METHOD(1) {
		split("", ':', output);
		ensure_equals(output.size(), 1u);
		ensure_equals(output[0], "");
	}
	
	TEST_METHOD(2) {
		split("hello world", ':', output);
		ensure_equals(output.size(), 1u);
		ensure_equals(output[0], "hello world");
	}
	
	TEST_METHOD(3) {
		split("hello world:foo bar", ':', output);
		ensure_equals(output.size(), 2u);
		ensure_equals(output[0], "hello world");
		ensure_equals(output[1], "foo bar");
	}
	
	TEST_METHOD(4) {
		split("hello world:", ':', output);
		ensure_equals(output.size(), 2u);
		ensure_equals(output[0], "hello world");
		ensure_equals(output[1], "");
	}
	
	TEST_METHOD(5) {
		split(":hello world", ':', output);
		ensure_equals(output.size(), 2u);
		ensure_equals(output[0], "");
		ensure_equals(output[1], "hello world");
	}
	
	TEST_METHOD(6) {
		split("abc:def::ghi", ':', output);
		ensure_equals(output.size(), 4u);
		ensure_equals(output[0], "abc");
		ensure_equals(output[1], "def");
		ensure_equals(output[2], "");
		ensure_equals(output[3], "ghi");
	}
	
	TEST_METHOD(7) {
		split("abc:::def", ':', output);
		ensure_equals(output.size(), 4u);
		ensure_equals(output[0], "abc");
		ensure_equals(output[1], "");
		ensure_equals(output[2], "");
		ensure_equals(output[3], "def");
	}
	
	
	/***** Test getSystemTempDir() *****/
	
	TEST_METHOD(11) {
		// It returns "/tmp" if the PASSENGER_TEMP_DIR environment is NULL.
		ensure_equals(string(getSystemTempDir()), "/tmp");
	}
	
	TEST_METHOD(12) {
		// It returns "/tmp" if the PASSENGER_TEMP_DIR environment is an empty string.
		setenv("PASSENGER_TEMP_DIR", "", 1);
		ensure_equals(string(getSystemTempDir()), "/tmp");
	}
	
	TEST_METHOD(13) {
		// It returns the value of the PASSENGER_TEMP_DIR environment if it is not NULL and not empty.
		setenv("PASSENGER_TEMP_DIR", "/foo", 1);
		ensure_equals(string(getSystemTempDir()), "/foo");
	}
	
	
	/***** Test BufferedUpload *****/
	
	TEST_METHOD(20) {
		// The resulting file handle is readable and writable.
		TempDir td("utils_test.tmp");
		BufferedUpload t("utils_test.tmp");
		char line[30];
		
		fprintf(t.handle, "hello world!");
		fflush(t.handle);
		fseek(t.handle, 0, SEEK_SET);
		memset(line, 0, sizeof(line));
		fgets(line, sizeof(line), t.handle);
		ensure_equals(string(line), "hello world!");
	}
	
	TEST_METHOD(21) {
		// It immediately unlinks the temp file.
		TempDir td("utils_test.tmp");
		BufferedUpload t("utils_test.tmp");
		ensure_equals(listDir("utils_test.tmp").size(), 0u);
	}
	
	/***** Test escapeForXml() *****/
	
	TEST_METHOD(25) {
		ensure_equals(escapeForXml(""), "");
		ensure_equals(escapeForXml("hello world"), "hello world");
		ensure_equals(escapeForXml("./hello_world/foo.txt"), "./hello_world/foo.txt");
		ensure_equals(escapeForXml("hello<world"), "hello&#60;world");
		ensure_equals(escapeForXml("hello\xFFworld"), "hello&#255;world");
		ensure_equals(escapeForXml("hello\xFF\xCCworld"), "hello&#255;&#204;world");
		ensure_equals(escapeForXml("hello\xFFworld\xCC"), "hello&#255;world&#204;");
	}
	
	/***** Test extractDirName() *****/
	
	TEST_METHOD(26) {
		ensure_equals("Test 1", extractDirName("/usr/lib"), "/usr");
		ensure_equals("Test 2", extractDirName("/usr/lib/"), "/usr");
		ensure_equals("Test 3", extractDirName("/usr/"), "/");
		ensure_equals("Test 4", extractDirName("usr"), ".");
		ensure_equals("Test 5", extractDirName("/"), "/");
		ensure_equals("Test 6", extractDirName("///"), "/");
		ensure_equals("Test 7", extractDirName("."), ".");
		ensure_equals("Test 8", extractDirName(".."), ".");
		ensure_equals("Test 9", extractDirName("./foo"), ".");
		ensure_equals("Test 10", extractDirName("../foo"), "..");
	}
	
	/***** Test resolveSymlink() *****/
	
	TEST_METHOD(27) {
		TempDir d("tmp.symlinks");
		system("touch tmp.symlinks/foo.txt");
		system("ln -s /usr/bin tmp.symlinks/absolute_symlink");
		system("ln -s foo.txt tmp.symlinks/file");
		system("ln -s file tmp.symlinks/file2");
		system("ln -s file2 tmp.symlinks/file3");
		ensure_equals(resolveSymlink("tmp.symlinks/file"), "tmp.symlinks/foo.txt");
		ensure_equals(resolveSymlink("tmp.symlinks/file2"), "tmp.symlinks/file");
		ensure_equals(resolveSymlink("tmp.symlinks/file3"), "tmp.symlinks/file2");
		ensure_equals(resolveSymlink("tmp.symlinks/absolute_symlink"), "/usr/bin");
	}
	
	/***** Test fillInMiddle() *****/
	
	TEST_METHOD(30) {
		ensure_equals(fillInMiddle(20, "server.", "123456", ".socket"), "server.123456.socket");
		ensure_equals(fillInMiddle(25, "server.", "123456", ".socket"), "server.123456.socket");
		ensure_equals(fillInMiddle(19, "server.", "123456", ".socket"), "server.12345.socket");
		ensure_equals(fillInMiddle(16, "server.", "123456", ".socket"), "server.12.socket");
		
		ensure_equals(fillInMiddle(10, "", "1234", ""), "1234");
		ensure_equals(fillInMiddle(4, "", "1234", ""), "1234");
		ensure_equals(fillInMiddle(2, "", "1234", ""), "12");
		
		ensure_equals(fillInMiddle(20, "", "1234", ".socket"), "1234.socket");
		ensure_equals(fillInMiddle(11, "", "1234", ".socket"), "1234.socket");
		ensure_equals(fillInMiddle(9, "", "1234", ".socket"), "12.socket");
		
		try {
			fillInMiddle(14, "server.", "123456", ".socket");
			fail();
		} catch (const ArgumentException &) { }
		
		try {
			fillInMiddle(10, "server.", "123456", ".socket");
			fail();
		} catch (const ArgumentException &) { }
		
		try {
			fillInMiddle(10, "server.", "", ".socket");
			fail();
		} catch (const ArgumentException &) { }
	}
	
	/***** Test MemZeroGuard *****/
	
	TEST_METHOD(31) {
		char buf[12] = "hello world";
		{
			MemZeroGuard g(buf, 2);
		}
		ensure(memcmp(buf, "\0\0llo world", sizeof(buf)) == 0);
	}
	
	TEST_METHOD(32) {
		string str("hello ");
		{
			MemZeroGuard g(str);
			str.append("world");
		}
		ensure(memcmp(str.c_str(), "\0\0\0\0\0\0\0\0\0\0\0", 11) == 0);
	}
	
	TEST_METHOD(33) {
		string str("hello ");
		{
			MemZeroGuard g(str);
			g.zeroNow();
			ensure(memcmp(str.c_str(), "\0\0\0\0\0\0", 6) == 0);
			str.append("world");
			ensure(memcmp(str.c_str(), "\0\0\0\0\0\0world", 11) == 0);
		}
		ensure(memcmp(str.c_str(), "\0\0\0\0\0\0\0\0\0\0\0", 11) == 0);
	}
	
	/***** Test parseModeString() *****/
	
	static bool modeStringCannotBeParsed(const StaticString &modeString) {
		try {
			parseModeString(modeString);
			return false;
		} catch (const InvalidModeStringException &) {
			return true;
		}
	}
	
	TEST_METHOD(36) {
		ensure_equals(parseModeString(""), 0);
		ensure_equals(parseModeString("u="), 0);
		ensure_equals(parseModeString("u=,u="), 0);
		ensure_equals(parseModeString("u=,g="), 0);
		ensure_equals(parseModeString("u=,g=,o="), 0);
		ensure_equals(parseModeString("u=,g=,o=,u=,g="), 0);
		ensure_equals(parseModeString("o="), 0);
	}
	
	TEST_METHOD(37) {
		ensure_equals("(1)", parseModeString("u=rwx"), S_IRWXU);
		ensure_equals("(2)", parseModeString("g=rwx"), S_IRWXG);
		ensure_equals("(3)", parseModeString("o=rwx"), S_IRWXO);
		ensure_equals("(4)", parseModeString("u=r,g=,o=rx"), S_IRUSR | S_IROTH | S_IXOTH);
		ensure_equals("(5)", parseModeString("o=r,g=wx"), S_IROTH | S_IWGRP | S_IXGRP);
		ensure_equals("(6)", parseModeString("u=r,g=w,o=x,u=x"), S_IRUSR | S_IXUSR | S_IWGRP | S_IXOTH);
		ensure_equals("(7)", parseModeString("u=rs,g=ws"), S_IRUSR | S_ISUID | S_IWGRP | S_ISGID);
	}
	
	TEST_METHOD(38) {
		ensure(modeStringCannotBeParsed("0"));
		ensure(modeStringCannotBeParsed("0600"));
		ensure(modeStringCannotBeParsed("600"));
		ensure(modeStringCannotBeParsed("x=rs"));
		ensure(modeStringCannotBeParsed("u=rs,g=rs,x=rs"));
		ensure(modeStringCannotBeParsed("x=rs"));
		ensure(modeStringCannotBeParsed("rwxrwxrwx"));
	}
	
	/***** Test makeDirTree() *****/
	
	TEST_METHOD(40) {
		// Creating a single subdirectory works.
		makeDirTree("tmp.dir/foo");
		ensure_equals(getFileType("tmp.dir/foo"), FT_DIRECTORY);
	}
	
	TEST_METHOD(41) {
		// Creating multiple subdirectories works.
		makeDirTree("tmp.dir/foo/bar");
		ensure_equals(getFileType("tmp.dir/foo"), FT_DIRECTORY);
		ensure_equals(getFileType("tmp.dir/foo/bar"), FT_DIRECTORY);
	}
	
	TEST_METHOD(42) {
		// It applies the permissions to all created directories.
		struct stat buf, buf2;
		
		stat("tmp.dir", &buf);
		makeDirTree("tmp.dir/foo/bar", "u=rwxs,g=,o=rx");
		stat("tmp.dir", &buf2);
		ensure_equals(buf.st_mode, buf2.st_mode);
		
		stat("tmp.dir/foo", &buf);
		stat("tmp.dir/foo/bar", &buf2);
		ensure_equals(buf.st_mode, buf2.st_mode);
		ensure_equals(buf.st_mode & 0xFFF,
			S_IRUSR | S_IWUSR | S_IXUSR | S_ISUID |
			S_IROTH | S_IXOTH);
	}
	
	TEST_METHOD(43) {
		// It correctly parses the permission string.
		testMakeDirTreeMode("empty 1", "", 0);
		testMakeDirTreeMode("empty 2", "u=", 0);
		testMakeDirTreeMode("empty 3", "g=", 0);
		testMakeDirTreeMode("empty 4", "o=", 0);
		testMakeDirTreeMode("empty 5", "u=,g=", 0);
		testMakeDirTreeMode("empty 6", "g=,o=", 0);
		
		testMakeDirTreeMode("(1)", "u=rwxs,g=rwxs,o=rwx",
			S_IRWXU | S_ISUID | S_IRWXG | S_ISGID | S_IRWXO);
		testMakeDirTreeMode("(2)", "u=s,g=rx,o=w",
			S_ISUID | S_IRGRP | S_IXGRP | S_IWOTH);
		testMakeDirTreeMode("(3)", "u=rwxs,g=,o=rwx",
			S_IRWXU | S_ISUID | S_IRWXO);
	}
	
	TEST_METHOD(44) {
		// It doesn't do anything if the directory already exists.
		struct stat buf, buf2;
		stat("tmp.dir", &buf);
		makeDirTree("tmp.dir");
		stat("tmp.dir", &buf2);
		ensure_equals(buf.st_mode, buf2.st_mode);
	}
	
	/***** Test stringToULL() *****/
	
	TEST_METHOD(47) {
		ensure_equals(stringToULL(""), 0ull);
		ensure_equals(stringToULL("0"), 0ull);
		ensure_equals(stringToULL("000"), 0ull);
		ensure_equals(stringToULL("1"), 1ull);
		ensure_equals(stringToULL("9"), 9ull);
		ensure_equals(stringToULL("010"), 10ull);
		ensure_equals(stringToULL("928"), 928ull);
		ensure_equals(stringToULL("2937104"), 2937104ull);
		ensure_equals(stringToULL("18446744073709551615"), 18446744073709551615ull);
		ensure_equals(stringToULL("    5abcdef1234"), 5ull);
	}
	
	/***** Test toHex() *****/
	
	TEST_METHOD(48) {
		char buf[sizeof(unsigned int) * 2 + 1];
		
		ensure_equals(toHex(0x0, buf), 1u);
		ensure(strcmp(buf, "0") == 0);
		
		ensure_equals(toHex(0x1, buf), 1u);
		ensure(strcmp(buf, "1") == 0);
		
		ensure_equals(toHex(0x9, buf), 1u);
		ensure(strcmp(buf, "9") == 0);
		
		ensure_equals(toHex(0xe, buf), 1u);
		ensure(strcmp(buf, "e") == 0);
		
		ensure_equals(toHex(0xdeadbeef, buf), 8u);
		ensure(strcmp(buf, "deadbeef") == 0);
		
		ensure_equals(toHex(0x1234f, buf), 5u);
		ensure(strcmp(buf, "1234f") == 0);
	}
}