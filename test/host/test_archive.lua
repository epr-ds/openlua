-- Archive host tests (run by test/host/run.sh through host_main).

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

local root = os.getenv("OPENLUA_ROOT") or ".."
local fixtures = root .. "/test/host/fixtures"
local work = "archive_lua"

files.delete(work)
files.mkdir(work)

local function find(list, name)
    if not list then return nil end
    for i = 1, #list do
        if list[i].name == name then return list[i] end
    end
    return nil
end

local function read(path)
    local f = io.open(path, "rb")
    if not f then return nil end
    local body = f:read("*a")
    f:close()
    return body
end

-- scan
local list, err = files.scan(fixtures .. "/archive.zip")
check(type(list) == "table", "zip scan returns table")
check(find(list, "hello.txt") ~= nil, "zip scan hello")
check(find(list, "hello.txt").size == 16, "zip scan hello size")
check(find(list, "dir") ~= nil and find(list, "dir").directory == true,
      "zip scan directory flag")
check(find(list, "stored.txt") ~= nil and find(list, "stored.txt").size == 18,
      "zip scan stored entry")

list = files.scan(fixtures .. "/archive.tar")
check(type(list) == "table" and find(list, "dir/nested.txt").size == 14,
      "tar scan nested")
local long = find(list, "very/long/path/that/needs/gnu/long/name/support/" ..
                        "abcdefghijklmnopqrstuvwxyz0123456789.txt")
check(long ~= nil and long.size == 12, "tar scan GNU long name")

list = files.scan(fixtures .. "/archive.tgz")
check(type(list) == "table" and #list == 5, "tar.gz scan count")

list = files.scan(fixtures .. "/hello.txt.gz")
check(type(list) == "table" and #list == 1, "gzip scan count")
check(list ~= nil and list[1].name == "hello.txt" and list[1].size == 13,
      "gzip scan member")

list = files.scan(fixtures .. "/archive.tar.bz2")
check(type(list) == "table" and #list == 5, "tar.bz2 scan count")

list = files.scan(fixtures .. "/hello.txt.bz2")
check(type(list) == "table" and #list == 1, "bzip2 scan count")
check(list ~= nil and list[1].name == "hello.txt" and list[1].size == 14,
      "bzip2 scan member")

-- extractfile
check(files.extractfile(fixtures .. "/archive.zip", "hello.txt",
                        work .. "/one.txt") == 1, "zip extractfile")
check(read(work .. "/one.txt") == "Hello, OpenLua!\n", "zip extractfile bytes")

check(files.extractfile(fixtures .. "/archive.tar", "dir/nested.txt",
                        work .. "/nested.txt") == 1, "tar extractfile")
check(read(work .. "/nested.txt") == "nested member\n", "tar extractfile bytes")

check(files.extractfile(fixtures .. "/hello.txt.gz", "hello.txt",
                        work .. "/gzip.txt") == 1, "gzip extractfile")
check(read(work .. "/gzip.txt") == "Hello, gzip!\n", "gzip extractfile bytes")

check(files.extractfile(fixtures .. "/hello.txt.bz2", "hello.txt",
                        work .. "/bz2.txt") == 1, "bzip2 extractfile")
check(read(work .. "/bz2.txt") == "Hello, bzip2!\n", "bzip2 extractfile bytes")

-- dst may be an existing directory
files.mkdir(work .. "/into")
check(files.extractfile(fixtures .. "/archive.zip", "stored.txt",
                        work .. "/into") == 1, "extractfile into directory")
check(read(work .. "/into/stored.txt") == "plain stored data\n",
      "extractfile directory bytes")

-- extract all
local n = files.extract(fixtures .. "/archive.zip", work .. "/all_zip")
check(n == 1, "zip extract all")
check(read(work .. "/all_zip/hello.txt") == "Hello, OpenLua!\n",
      "zip extract all hello")
check(read(work .. "/all_zip/dir/nested.txt") == "nested member\n",
      "zip extract all nested")
check(files.exists(work .. "/all_zip/dir"), "zip extract all directory")

check(files.extract(fixtures .. "/archive.tgz", work .. "/all_tgz") == 1,
      "tar.gz extract all")
check(read(work .. "/all_tgz/hello.txt") == "Hello, OpenLua!\n",
      "tar.gz extract all hello")

check(files.extract(fixtures .. "/hello.txt.gz", work .. "/all_gzip") == 1,
      "gzip extract all")
check(read(work .. "/all_gzip/hello.txt") == "Hello, gzip!\n",
      "gzip extract all bytes")

check(files.extract(fixtures .. "/archive.tar.bz2", work .. "/all_tbz2") == 1,
      "tar.bz2 extract all")
check(read(work .. "/all_tbz2/hello.txt") == "Hello, OpenLua!\n",
      "tar.bz2 extract all hello")

check(files.extract(fixtures .. "/hello.txt.bz2", work .. "/all_bz2") == 1,
      "bzip2 extract all")
check(read(work .. "/all_bz2/hello.txt") == "Hello, bzip2!\n",
      "bzip2 extract all bytes")

-- synchronous OneLua thread aliases
check(files.threadextractfile(fixtures .. "/archive.zip", "hello.txt",
                              work .. "/thread.txt") == 1, "threadextractfile")
check(read(work .. "/thread.txt") == "Hello, OpenLua!\n",
      "threadextractfile bytes")
check(files.threadextract(fixtures .. "/archive.zip", work .. "/all_thread") == 1,
      "threadextract extract all")

-- failures return 0 plus a reason
local rc, reason = files.extractfile(fixtures .. "/archive.zip", "missing.txt",
                                     work .. "/missing.txt")
check(rc == 0, "missing member returns 0")
check(type(reason) == "string" and #reason > 0, "missing member reason")

local bad, badreason = files.scan(fixtures .. "/archive.zip.nope")
check(bad == nil and type(badreason) == "string", "missing archive scan")

local f = assert(io.open(work .. "/fake.rar", "wb"))
f:write("Rar!\26\7\0")
f:close()
local rar, rarreason = files.scan(work .. "/fake.rar")
check(rar == nil and rarreason:find("unsupported") ~= nil,
      "rar scan reports unsupported")
check(files.extract(work .. "/fake.rar", work .. "/rar") == 0,
      "rar extract returns 0")

-- extracting an archive over itself must not leave a partial file
check(files.extract(fixtures .. "/archive.zip", work .. "/all_zip") == 1,
      "archive extract can be repeated")

files.delete(work)
print(string.format("archive Lua: %d assertions, %d failure(s)", ok, fails))
if fails > 0 then os.exit(1) end
