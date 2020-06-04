/*!
 * @brief Simple Brainf**k Compiler for x64 PE
 *
 * @author  koturn
 * @date    2020 05/31
 * @version 1.0
 */
#include <cstdint>
#include <ctime>
#include <algorithm>
#include <array>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <stack>
#include <string>
#include <type_traits>

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>


namespace
{
//! .textのアドレス
constexpr ::ULONGLONG kBaseAddr = 0x00400000;
//! パディング含むPE headerのサイズ
constexpr ::DWORD kPeHeaderSizeWithPadding = 0x0200;
//! パディング含む.idataのサイズ
constexpr ::DWORD kIdataSizeWithPadding = 0x0200;
//! DOSスタブ
constexpr char kDosStub[] =
  "\x0e"  // push cs
  "\x1f"  // pop ds
  "\xba\x0e\x00"  // mov dx, 0x000e (Offset to message data from here)
  "\xb4\x09"  // mov ah, 0x09 (Argument for int 0x21: print)
  "\xcd\x21"  // int 0x21
  "\xb8\x01\x4c"  // mov ax, 0x4c01 (Argument for int 0x21: exit)
  "\xcd\x21"  // int 0x21
  "This program cannot be run in DOS mode.\r\r\n$\x00\x00\x00\x00\x00\x00";

//! インポートするDLLの名前
constexpr char kDllName[] = "msvcrt.dll\0\0\0\0\0";
//! putcharの関数名
constexpr char kPutcharName[] = "putchar";
//! getcharの関数名
constexpr char kGetcharName[] = "getchar";
//! exitの関数名
constexpr char kExitName[] = "exit\0\0\0";
//! コードのアラインメント
constexpr std::size_t kCodeAlignment = 0x1000;
//! Brainfuckの文字
const std::string kBrainfuckChars{"><+-.,[]"};


/*!
 * @brief アラインメントを考慮したサイズを計算する
 *
 * @tparam U サイズの型（整数型であること）
 * @tparam V アラインメントの型（整数型であること）
 * @param [in] size  元のサイズ
 * @param [in] alignment  アラインメント
 * @return アラインメントを考慮したサイズ
 */
template <
  typename U,
  typename V
>
inline constexpr U
calcAlignedSize(U size, V alignment) noexcept
{
  static_assert(std::is_integral<U>::value, "[calcAlignedSize] The first template parameter must be an integral");
  static_assert(std::is_integral<V>::value, "[calcAlignedSize] The second template parameter must be an integral");
  return static_cast<U>(alignment * ((size + alignment - 1) / alignment));
}


/*!
 * @brief 複数のバイト列を指定したファイルストリームに書き込む
 *
 * @param [in] ofs  書き込み先ファイルストリーム
 * @param [in] data  バイト列
 */
inline void
writeBytes(std::ofstream& ofs, const std::initializer_list<std::uint8_t>& data)
{
  ofs.write(reinterpret_cast<const char*>(data.begin()), data.size());
}


/*!
 * @brief 指定したデータをファイルストリームに書き込む
 *
 * @param [in] ofs  書き込み先ファイルストリーム
 * @param [in] data  書き込むデータ
 */
template <typename T>
inline void
writeAs(std::ofstream& ofs, const T& data)
{
  ofs.write(reinterpret_cast<const char*>(&data), sizeof(data));
}


/*!
 * @brief ヘッダ部分の書き込みを行う
 *
 * @param [in] ofs  書き込み先ファイルストリーム
 * @param [in] codeSizeWithPadding  コード部分のサイズ（パディングあり） (byte単位)
 */
inline void
writeHeader(std::ofstream& ofs, std::size_t codeSize, std::size_t exitAddrPos)
{
  const auto codeSizeWithPadding = calcAlignedSize(codeSize, kCodeAlignment);

  // Write DOS header
  ::IMAGE_DOS_HEADER idh;
  idh.e_magic = IMAGE_DOS_SIGNATURE;
  idh.e_cblp = 0x0090;
  idh.e_cp = 0x0003;
  idh.e_crlc = 0x0000;
  idh.e_cparhdr = 0x0004;
  idh.e_minalloc = 0x0000;
  idh.e_maxalloc = 0xffff;
  idh.e_ss = 0x0000;
  idh.e_sp = 0x00b8;
  idh.e_csum = 0x0000;
  idh.e_ip = 0x0000;
  idh.e_cs = 0x0000;
  idh.e_lfarlc = 0x0040;
  idh.e_ovno = 0x0000;
  std::fill(std::begin(idh.e_res), std::end(idh.e_res), 0x0000);
  idh.e_oemid = 0x0000;
  idh.e_oeminfo = 0x0000;
  std::fill(std::begin(idh.e_res2), std::end(idh.e_res2), 0x0000);
  idh.e_lfanew = 0x00000080;
  writeAs(ofs, idh);

  // Write DOS stub
  writeAs(ofs, kDosStub);

  writeAs<::DWORD>(ofs, IMAGE_NT_SIGNATURE);

  const auto ts = std::time(nullptr);

  // Write image file header
  ::IMAGE_FILE_HEADER ifh;
  ifh.Machine = IMAGE_FILE_MACHINE_AMD64;  // 0x8664
  ifh.NumberOfSections = 3;
  // 格好をつけるためにタイムスタンプを入れているが，0でもよい
  ifh.TimeDateStamp = ts;
  ifh.PointerToSymbolTable = 0;
  ifh.NumberOfSymbols = 0;
  ifh.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
  ifh.Characteristics = IMAGE_FILE_RELOCS_STRIPPED
    | IMAGE_FILE_EXECUTABLE_IMAGE
    | IMAGE_FILE_LINE_NUMS_STRIPPED
    | IMAGE_FILE_LOCAL_SYMS_STRIPPED
    | IMAGE_FILE_DEBUG_STRIPPED;
  writeAs(ofs, ifh);

  ::IMAGE_OPTIONAL_HEADER64 ioh;
  ioh.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
  // 値は0でもよい
  // 14.26は5/31現在のMSVCの最新のリンカのバージョン
  ioh.MajorLinkerVersion = 14;
  ioh.MinorLinkerVersion = 26;
  ioh.SizeOfCode = codeSize;
  ioh.SizeOfInitializedData = 0;
  ioh.SizeOfUninitializedData = 65536;
  ioh.AddressOfEntryPoint = 0x1000;
  ioh.BaseOfCode = 0x1000;
  ioh.ImageBase = kBaseAddr;
  ioh.SectionAlignment = 0x1000;
  ioh.FileAlignment = 0x0200;
  // 値は0でもよい
  // 6.0はWindows Vistaを示す
  ioh.MajorOperatingSystemVersion = 6;
  ioh.MinorOperatingSystemVersion = 0;
  ioh.MajorImageVersion = 0;
  ioh.MinorImageVersion = 0;
  // 値は0でもよい
  // 6.0はWindows Vistaを示す
  ioh.MajorSubsystemVersion = 6;
  ioh.MinorSubsystemVersion = 0;
  ioh.Win32VersionValue = 0;  // Not used. Always 0
  ioh.SizeOfImage = 0x10000 + codeSizeWithPadding + ioh.SectionAlignment * 2;
  ioh.SizeOfHeaders = kPeHeaderSizeWithPadding;
  ioh.CheckSum = 0;
  ioh.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
  ioh.DllCharacteristics = 0;
  ioh.SizeOfStackReserve = 1024 * 1024;
  ioh.SizeOfStackCommit = 8 * 1024;
  ioh.SizeOfHeapReserve = 1024 * 1024;
  ioh.SizeOfHeapCommit = 4 * 1024;
  ioh.LoaderFlags = 0;
  ioh.NumberOfRvaAndSizes = 16;
  std::fill(std::begin(ioh.DataDirectory), std::end(ioh.DataDirectory), ::IMAGE_DATA_DIRECTORY{0, 0});
  ioh.DataDirectory[1].VirtualAddress = ioh.BaseOfCode + codeSizeWithPadding;  // import table
  ioh.DataDirectory[1].Size = 100;
  writeAs(ofs, ioh);

  // .text section
  ::IMAGE_SECTION_HEADER ishText;
  std::copy_n(".text\0\0", sizeof(ishText.Name), ishText.Name);
  ishText.Misc.VirtualSize = codeSize;
  ishText.VirtualAddress = ioh.BaseOfCode;
  ishText.SizeOfRawData = codeSize;
  ishText.PointerToRawData = kPeHeaderSizeWithPadding + kIdataSizeWithPadding;
  ishText.PointerToRelocations = 0x00000000;
  ishText.PointerToLinenumbers = 0x00000000;
  ishText.NumberOfRelocations = 0x0000;
  ishText.NumberOfLinenumbers = 0x0000;
  ishText.Characteristics = IMAGE_SCN_CNT_CODE
    | IMAGE_SCN_ALIGN_16BYTES
    | IMAGE_SCN_MEM_EXECUTE
    | IMAGE_SCN_MEM_READ;
  writeAs(ofs, ishText);

  // .idata section
  ::IMAGE_SECTION_HEADER ishIdata;
  std::copy_n(".idata\0", sizeof(ishIdata.Name), ishIdata.Name);
  ishIdata.Misc.VirtualSize = 100;
  ishIdata.VirtualAddress = ishText.VirtualAddress + codeSizeWithPadding;
  ishIdata.SizeOfRawData = 512;
  ishIdata.PointerToRawData = kPeHeaderSizeWithPadding;
  ishIdata.PointerToRelocations = 0x00000000;
  ishIdata.PointerToLinenumbers = 0x00000000;
  ishIdata.NumberOfRelocations = 0x0000;
  ishIdata.NumberOfLinenumbers = 0x00000;
  ishIdata.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA
    | IMAGE_SCN_ALIGN_4BYTES
    | IMAGE_SCN_MEM_READ;
  writeAs(ofs, ishIdata);

  // .bss section
  ::IMAGE_SECTION_HEADER ishBss;
  std::copy_n(".bss\0\0\0", sizeof(ishBss.Name), ishBss.Name);
  ishBss.Misc.VirtualSize = 65536;
  ishBss.VirtualAddress = ishIdata.VirtualAddress + ioh.SectionAlignment;
  ishBss.SizeOfRawData = 0;
  ishBss.PointerToRawData = 0;
  ishBss.PointerToRelocations = 0x00000000;
  ishBss.PointerToLinenumbers = 0x00000000;
  ishBss.NumberOfRelocations = 0x0000;
  ishBss.NumberOfLinenumbers = 0x00000;
  ishBss.Characteristics = IMAGE_SCN_CNT_UNINITIALIZED_DATA
    | IMAGE_SCN_ALIGN_8BYTES
    | IMAGE_SCN_MEM_READ
    | IMAGE_SCN_MEM_WRITE;
  writeAs(ofs, ishBss);

  ofs.seekp(kPeHeaderSizeWithPadding, std::ios_base::beg);

  std::array<::IMAGE_IMPORT_DESCRIPTOR, 2> iids;
  std::array<::IMAGE_THUNK_DATA64, 4> itdInts;

  iids[0].OriginalFirstThunk = static_cast<::DWORD>(ishIdata.VirtualAddress + sizeof(iids));  // int
  iids[0].TimeDateStamp = ts;
  iids[0].ForwarderChain = 0x00000000;
  iids[0].Name = static_cast<::DWORD>(iids[0].OriginalFirstThunk + sizeof(itdInts));  // msvcrt.dll
  iids[0].FirstThunk = iids[0].Name + 16;  // iat
  iids[1].Characteristics = 0x00000000;
  iids[1].TimeDateStamp = ts;
  iids[1].ForwarderChain = 0x00000000;
  iids[1].Name = 0x00000000;
  iids[1].FirstThunk = 0x00000000;
  writeAs(ofs, iids);

  itdInts[0].u1.AddressOfData = ishIdata.VirtualAddress + sizeof(iids) + sizeof(kDllName) + sizeof(itdInts) * 2;  // putchar
  itdInts[1].u1.AddressOfData = itdInts[0].u1.AddressOfData + sizeof(::WORD) + sizeof(kPutcharName);  // getchar
  itdInts[2].u1.AddressOfData = itdInts[1].u1.AddressOfData + sizeof(::WORD) + sizeof(kGetcharName);  // exit
  itdInts[3].u1.AddressOfData = 0x00000000;
  writeAs(ofs, itdInts);  // write INT (Import Name Table)
  writeAs(ofs, kDllName);
  writeAs(ofs, itdInts);  // IAT (Import Address Table) is same as INT

  writeAs<::WORD>(ofs, 0x0000);
  writeAs(ofs, kPutcharName);
  writeAs<::WORD>(ofs, 0x0000);
  writeAs(ofs, kGetcharName);
  writeAs<::WORD>(ofs, 0x0000);
  writeAs(ofs, kExitName);

  // Fill putchar() address
  ofs.seekp(ishText.PointerToRawData + 0x07, std::ios_base::beg);
  writeAs<std::uint32_t>(ofs, ioh.ImageBase + iids[0].FirstThunk);
  // Fill getchar() address
  ofs.seekp(ishText.PointerToRawData + 0x0f, std::ios_base::beg);
  writeAs<std::uint32_t>(ofs, ioh.ImageBase + iids[0].FirstThunk + sizeof(::ULONGLONG));
  // Fill exit() address
  ofs.seekp(exitAddrPos, std::ios_base::beg);
  writeAs<std::uint32_t>(ofs, ioh.ImageBase + iids[0].FirstThunk + sizeof(::ULONGLONG) * 2);
  // Fill .bss address
  ofs.seekp(ishText.PointerToRawData + 0x16, std::ios_base::beg);
  writeAs<std::uint32_t>(ofs, ioh.ImageBase + ishBss.VirtualAddress);
}


/*!
 * @brief 文字列の指定したオフセットから指定文字が何個連続するか数える
 *
 * @param [in] str  対象文字列
 * @param [in] ch  連え対象の文字列
 * @param [in] offset  数え始めるオフセット
 * @return 連続する文字数
 */
inline int
countSuccChars(const std::string& str, char ch, std::string::size_type offset)
{
  int cnt = 0;
  for (auto i = offset; i < str.size() && str[i] == ch; i++) {
    cnt++;
  }
  return cnt;
}
}  // namespace


/*!
 * @brief このプログラムのエントリポイント
 * @return  終了ステータス
 */
int
main()
{
#ifdef _MSC_VER
  // Brainf**kのソースファイルのパス
  constexpr auto srcFilePath = "source.bf";
  // 出力ファイルのパス
  constexpr auto dstFilePath = "a.exe";
#else
  // Brainf**kのソースファイルのパス (出力ファイルに合わせて"./"を付与したが無くてもいい)
  constexpr auto srcFilePath = "./source.bf";
  // 出力ファイルのパス (後に std::system でも使用するので，"./" を付与している)
  constexpr auto dstFilePath = "./a.exe";
#endif

  std::ifstream ifs{srcFilePath};
  if (!ifs) {
    std::cerr << "Failed to open " << srcFilePath << std::endl;
    return 1;
  }
  std::string source{std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{}};
  ifs.close();

  std::ofstream ofs{dstFilePath, std::ios::binary};
  if (!ofs) {
    std::cerr << "Failed to open " << dstFilePath << std::endl;
    return 1;
  }

  // ヘッダ部分は一旦飛ばす（後に書き込む）
  ofs.seekp(kPeHeaderSizeWithPadding + kIdataSizeWithPadding, std::ios_base::beg);

  // push rsi
  // push rdi
  // push rbp
  writeBytes(ofs, {0x56, 0x57, 0x55});
  // mov rsi,ds:{0x********}  # putchar() address
  writeBytes(ofs, {0x48, 0x8b, 0x34, 0x25});
  writeAs<std::uint32_t>(ofs, 0x00000000);  // Fill later
  // mov rdi,ds:{0x********}  # getchar() address
  writeBytes(ofs, {0x48, 0x8b, 0x3c, 0x25});
  writeAs<std::uint32_t>(ofs, 0x00000000);  // Fill later
  // mov rbx, {0x********}  # .bss address
  writeBytes(ofs, {0x48, 0xc7, 0xc3});
  writeAs<std::uint32_t>(ofs, 0x00000000);  // Fill later

  // 連続文字のカウント等を楽にするために予めBrainfuckに関係しない文字を取り除く
  source.erase(
    std::remove_if(
      std::begin(source),
      std::end(source),
      [](const auto& e) {
        return kBrainfuckChars.find(e) == std::string::npos;
      }),
    std::end(source));

  std::stack<std::ostream::pos_type> loopStack;
  for (decltype(source)::size_type i = 0; i < source.size(); i++) {
    switch (source[i]) {
      case '>':
        {
          const auto cnt = countSuccChars(source, '>', i + 1) + 1;
          i += cnt - 1;
          if (cnt > 127) {
            // add rbx, {cnt}
            writeBytes(ofs, {0x48, 0x81, 0xc3});
            writeAs<std::uint32_t>(ofs, cnt);
          } else if (cnt > 1) {
            // add rbx, {cnt}
            writeBytes(ofs, {0x48, 0x83, 0xc3});
            writeAs<std::uint8_t>(ofs, cnt);
          } else {
            // inc rbx
            writeBytes(ofs, {0x48, 0xff, 0xc3});
          }
        }
        break;
      case '<':
        {
          const auto cnt = countSuccChars(source, '<', i + 1) + 1;
          i += cnt - 1;
          if (cnt > 127) {
            // sub rbx, {cnt}
            writeBytes(ofs, {0x48, 0x81, 0xeb});
            writeAs<std::uint32_t>(ofs, cnt);
          } else if (cnt > 1) {
            // sub rbx, {cnt}
            writeBytes(ofs, {0x48, 0x83, 0xeb});
            writeAs<std::uint8_t>(ofs, cnt);
          } else {
            // dec rbx
            writeBytes(ofs, {0x48, 0xff, 0xcb});
          }
        }
        break;
      case '+':
        {
          auto cnt = countSuccChars(source, '+', i + 1) + 1;
          i += cnt - 1;
          cnt %= 256;
          if (cnt > 1) {
            // add byte ptr [rbx], {cnt}
            writeBytes(ofs, {0x80, 0x03});
            writeAs<std::uint8_t>(ofs, cnt);
          } else if (cnt == 1) {
            // inc byte ptr [rbx]
            writeBytes(ofs, {0xfe, 0x03});
          }
        }
        break;
      case '-':
        {
          auto cnt = countSuccChars(source, '-', i + 1) + 1;
          i += cnt - 1;
          cnt %= 256;
          if (cnt > 1) {
            // sub byte ptr [rbx], {cnt}
            writeBytes(ofs, {0x80, 0x2b});
            writeAs<std::uint8_t>(ofs, cnt);
          } else if (cnt == 1) {
            // dec byte ptr [rbx]
            writeBytes(ofs, {0xfe, 0x0b});
          }
        }
        break;
      case '.':
        // mov rcx, byte ptr [rbx]
        writeBytes(ofs, {0x48, 0x8b, 0x0b});
        // sub rsp, 0x20
        writeBytes(ofs, {0x48, 0x83, 0xec});
        writeAs<std::uint8_t>(ofs, 0x20);
        // call rsi
        writeBytes(ofs, {0xff, 0xd6});
        // add rsp, 0x20
        writeBytes(ofs, {0x48, 0x83, 0xc4});
        writeAs<std::uint8_t>(ofs, 0x20);
        break;
      case ',':
        // sub rsp, 0x20
        writeBytes(ofs, {0x48, 0x83, 0xec});
        writeAs<std::uint8_t>(ofs, 0x20);
        // call rdi
        writeBytes(ofs, {0xff, 0xd7});
        // add rsp, 0x20
        writeBytes(ofs, {0x48, 0x83, 0xc4});
        writeAs<std::uint8_t>(ofs, 0x20);
        // mov byte ptr [rbx], al
        writeBytes(ofs, {0x88, 0x03});
        break;
      case '[':
        // [-] または [+] はゼロ代入にする
        if (i + 2 < source.size()
            && (source[i + 1] == '+' || source[i + 1] == '-')
            && source[i + 2] == ']') {
          // mov byte ptr [rbx], 0x00
          writeBytes(ofs, {0xc6, 0x03, 0x00});
          i += 2;
        } else {
          loopStack.push(ofs.tellp());
          // cmp byte ptr [rbx], 0x00
          writeBytes(ofs, {0x80, 0x3b});
          writeAs<std::uint8_t>(ofs, 0x00);
          // je 0x********
          writeBytes(ofs, {0x0f, 0x84});
          writeAs<std::uint32_t>(ofs, 0x00000000);
        }
        break;
      case ']':
        if (loopStack.empty()) {
          std::cerr << "'[' corresponding to ']' is not found." << std::endl;
          return 1;
        }
        {
          const auto pos = loopStack.top();
          const auto offset = static_cast<int>(pos - ofs.tellp()) - 1;
          // 一律near jumpでもいいけど，一応short jumpも生成するようにしてある
          if (offset - static_cast<int>(sizeof(std::uint8_t)) < -128) {
            // jmp {offset} (near jump)
            writeAs<std::uint8_t>(ofs, 0xe9);
            writeAs<std::uint32_t>(ofs, offset - sizeof(std::uint32_t));
          } else {
            // jmp {offset} (short jump)
            writeAs<std::uint8_t>(ofs, 0xeb);
            writeAs<std::uint8_t>(ofs, offset - sizeof(std::uint8_t));
          }
          // fill loop start
          const auto curPos = ofs.tellp();
          ofs.seekp(pos + decltype(pos){5}, std::ios_base::beg);
          writeAs<std::uint32_t>(ofs, curPos - ofs.tellp() - sizeof(std::uint32_t));
          ofs.seekp(curPos, std::ios_base::beg);
          loopStack.pop();
        }
        break;
      default:
        break;
    }
  }

  if (!loopStack.empty()) {
    std::cerr << "']' corresponding to '[' is not found." << std::endl;
    return 1;
  }

  // pop rsi
  // pop rdi
  // pop rbp
  writeBytes(ofs, {0x5d, 0x5f, 0x5e});
#if 0
  // xor ecx, ecx
  writeBytes(ofs, {0x31, 0xc9});
  // mov rsi, ds:{0x********}  # exit
  writeBytes(ofs, {0x48, 0x8b, 0x34, 0x25});
  const auto exitAddrPos = ofs.tellp();
  writeAs<std::uint32_t>(ofs, 0x00000000);  // Fill later
  // sub rsp, 0x20
  writeBytes(ofs, {0x48, 0x83, 0xec});
  writeAs<std::uint8_t>(ofs, 0x20);
  // call rsi
  writeBytes(ofs, {0xff, 0xd6});
#else
  // xor rax, rax
  writeBytes(ofs, {0x48, 0x31, 0xc0});
  // retq
  writeAs<std::uint8_t>(ofs, 0xc3);
  //  d:	48 31 c0             	xor    %rax,%rax
  // 10:	c3                   	retq
  // 11:	b8 00 00 00 00       	mov    $0x0,%eax
  // 16:	48 83 c4 20          	add    $0x20,%rsp
  // 1a:	5d                   	pop    %rbp
  // 1b:	c3                   	retq
  const auto exitAddrPos = ofs.tellp();
  writeAs<std::uint32_t>(ofs, 0x00000000);  // Fill later
#endif

  const auto codeSize = static_cast<std::size_t>(ofs.tellp()) - (kPeHeaderSizeWithPadding + kIdataSizeWithPadding);
  const auto codeSizeWithPadding = calcAlignedSize(codeSize, kCodeAlignment);
  // Write padding
  ofs.seekp(codeSizeWithPadding - codeSize - 1, std::ios_base::cur);
  writeAs<std::uint8_t>(ofs, 0x00);

  // Write header
  ofs.seekp(0, std::ios_base::beg);
  writeHeader(ofs, codeSize, exitAddrPos);
  ofs.seekp(0, std::ios_base::end);

  ofs.close();

  std::system(dstFilePath);
}
