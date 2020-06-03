/*!
 * @brief Simple Brainf**k Compiler for x64 ELF
 *
 * @author  koturn
 * @date    2020 05/30
 * @version 1.0
 */
#if __cplusplus >= 201703L && defined(__has_include) && __has_include(<filesystem>)
#  define HAS_HEADER_FILESYSTEM 1
#endif


#include <cstdint>
#include <algorithm>
#ifdef HAS_HEADER_FILESYSTEM
#  include <filesystem>
#endif
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stack>
#include <string>

#include <elf.h>
#ifndef HAS_HEADER_FILESYSTEM
#  include <sys/stat.h>
#endif


namespace
{
//! .textセクションのアドレス
constexpr ::Elf64_Addr kBaseAddr = 0x04048000;
//! .bssセクションのアドレス
constexpr ::Elf64_Addr kBssAddr = 0x04248000;
//! プログラムヘッダ数
constexpr ::Elf64_Half kNProgramHeaders = 2;
//! セクションヘッダ数
constexpr ::Elf64_Half kNSectionHeaders = 4;
//! ヘッダ部分のサイズ
constexpr ::Elf64_Off kHeaderSize = sizeof(::Elf64_Ehdr) + sizeof(::Elf64_Phdr) * kNProgramHeaders;
//! フッタ部分のサイズ
constexpr ::Elf64_Off kFooterSize = sizeof(::Elf64_Shdr) * kNSectionHeaders;
//! 文字列テーブル
constexpr char kShStrTab[] = "\0.text\0.shstrtab\0.bss";
//! Brainfuckの文字
const std::string kBrainfuckChars{"><+-.,[]"};


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
 * @param [in] codeSize  コード部分のサイズ (byte単位)
 */
inline void
writeHeader(std::ofstream& ofs, std::size_t codeSize)
{
  // ELF header
  ::Elf64_Ehdr ehdr;
  std::fill(std::begin(ehdr.e_ident), std::end(ehdr.e_ident), 0x00);
  ehdr.e_ident[EI_MAG0] = ELFMAG0;
  ehdr.e_ident[EI_MAG1] = ELFMAG1;
  ehdr.e_ident[EI_MAG2] = ELFMAG2;
  ehdr.e_ident[EI_MAG3] = ELFMAG3;
  ehdr.e_ident[EI_CLASS] = ELFCLASS64;
  ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
  ehdr.e_ident[EI_VERSION] = EV_CURRENT;
  ehdr.e_ident[EI_OSABI] = ELFOSABI_LINUX;
  ehdr.e_ident[EI_ABIVERSION] = 0x00;
  ehdr.e_ident[EI_PAD] = 0x00;
  ehdr.e_type = ET_EXEC;
  ehdr.e_machine = EM_X86_64;
  ehdr.e_version = EV_CURRENT;
  ehdr.e_entry = kBaseAddr + kHeaderSize;
  ehdr.e_phoff = sizeof(::Elf64_Ehdr);
  ehdr.e_shoff = kHeaderSize + sizeof(kShStrTab) + codeSize;
  ehdr.e_flags = 0x00000000;
  ehdr.e_ehsize = sizeof(::Elf64_Ehdr);
  ehdr.e_phentsize = sizeof(::Elf64_Phdr);
  ehdr.e_phnum = kNProgramHeaders;
  ehdr.e_shentsize = sizeof(::Elf64_Shdr);
  ehdr.e_shnum = kNSectionHeaders;
  ehdr.e_shstrndx = 1;
  writeAs(ofs, ehdr);

  // Program header
  ::Elf64_Phdr phdr;
  phdr.p_type = PT_LOAD;
  phdr.p_flags = PF_R | PF_X;
  phdr.p_offset = 0x0000000000000000;
  phdr.p_vaddr = kBaseAddr;
  phdr.p_paddr = kBaseAddr;
  phdr.p_filesz = kHeaderSize + sizeof(kShStrTab) + kFooterSize + codeSize;
  phdr.p_memsz = kHeaderSize + sizeof(kShStrTab) + kFooterSize + codeSize;
  phdr.p_align = 0x0000000000001000;
  writeAs(ofs, phdr);

  // Program header for .bss
  ::Elf64_Phdr phdrBss;
  phdrBss.p_type = PT_LOAD;
  phdrBss.p_flags = PF_R | PF_W;
  phdrBss.p_offset = 0x0000000000000000;
  phdrBss.p_vaddr = kBssAddr;
  phdrBss.p_paddr = kBssAddr;
  phdrBss.p_filesz = 0x0000000000000000;
  phdrBss.p_memsz = 0x0000000000010000;
  phdrBss.p_align = 0x0000000000001000;
  writeAs(ofs, phdrBss);
}


/*!
 * @brief フッタ部分の書き込みを行う
 *
 * @param [in] ofs  書き込み先ファイルストリーム
 * @param [in] codeSize  コード部分のサイズ (byte単位)
 */
inline void
writeFooter(std::ofstream& ofs, std::size_t codeSize)
{
  writeAs(ofs, kShStrTab);

  // First section header
  ::Elf64_Shdr shdr;
  shdr.sh_name = 0;
  shdr.sh_type = SHT_NULL;
  shdr.sh_flags = 0x0000000000000000;
  shdr.sh_addr = 0x0000000000000000;
  shdr.sh_offset = 0x0000000000000000;
  shdr.sh_size = 0x0000000000000000;
  shdr.sh_link = 0x00000000;
  shdr.sh_info = 0x00000000;
  shdr.sh_addralign = 0x0000000000000000;
  shdr.sh_entsize = 0x0000000000000000;
  writeAs(ofs, shdr);

  // Second section header (.shstrtab)
  ::Elf64_Shdr shdrShstrtab;
  shdrShstrtab.sh_name = 7;
  shdrShstrtab.sh_type = SHT_STRTAB;
  shdrShstrtab.sh_flags = 0x0000000000000000;
  shdrShstrtab.sh_addr = 0x0000000000000000;
  shdrShstrtab.sh_offset = kHeaderSize + codeSize;
  shdrShstrtab.sh_size = sizeof(kShStrTab);
  shdrShstrtab.sh_link = 0x00000000;
  shdrShstrtab.sh_info = 0x00000000;
  shdrShstrtab.sh_addralign = 0x0000000000000001;
  shdrShstrtab.sh_entsize = 0x0000000000000000;
  writeAs(ofs, shdrShstrtab);

  // Third section header (.text)
  ::Elf64_Shdr shdrText;
  shdrText.sh_name = 1;
  shdrText.sh_type = SHT_PROGBITS;
  shdrText.sh_flags = SHF_EXECINSTR | SHF_ALLOC;
  shdrText.sh_addr = kBaseAddr + kHeaderSize;
  shdrText.sh_offset = kHeaderSize;
  shdrText.sh_size = codeSize;
  shdrText.sh_link = 0x00000000;
  shdrText.sh_info = 0x00000000;
  shdrText.sh_addralign = 0x0000000000000004;
  shdrText.sh_entsize = 0x0000000000000000;
  writeAs(ofs, shdrText);

  // Fourth section header (.bss)
  ::Elf64_Shdr shdrBss;
  shdrBss.sh_name = 17;
  shdrBss.sh_type = SHT_NOBITS;
  shdrBss.sh_flags = SHF_ALLOC | SHF_WRITE;
  shdrBss.sh_addr = kBssAddr;
  shdrBss.sh_offset = 0x0000000000001000;
  shdrBss.sh_size = 0x0000000000010000;  // 65536 cells
  shdrBss.sh_link = 0x00000000;
  shdrBss.sh_info = 0x00000000;
  shdrBss.sh_addralign = 0x0000000000000010;
  shdrBss.sh_entsize = 0x0000000000000000;
  writeAs(ofs, shdrBss);
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
  // Brainf**kのソースファイルのパス (出力ファイルに合わせて"./"を付与したが無くてもいい)
  constexpr auto srcFilePath = "./source.bf";
  // 出力ファイルのパス (後に std::system でも使用するので，"./" を付与している)
  constexpr auto dstFilePath = "./a.out";

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

  // 連続文字等のカウントを楽にするために予めBrainfuckに関係しない文字を取り除く
  source.erase(
    std::remove_if(
      std::begin(source),
      std::end(source),
      [](const auto& e) {
        return kBrainfuckChars.find(e) == std::string::npos;
      }),
    std::end(source));

  // ヘッダ部分は一旦飛ばす（後に書き込む）
  ofs.seekp(kHeaderSize, std::ios_base::beg);

  // movabs rsi, {kBssAddr}
  writeBytes(ofs, {0x48, 0xbe});
  writeAs(ofs, kBssAddr);
  // mov edx, 0x01
  writeBytes(ofs, {0xba});
  writeAs<std::uint32_t>(ofs, 0x00000001);

  std::stack<std::ostream::pos_type> loopStack;
  for (decltype(source)::size_type i = 0; i < source.size(); i++) {
    switch (source[i]) {
      case '>':
        {
          const auto cnt = countSuccChars(source, '>', i + 1) + 1;
          i += cnt - 1;
          if (cnt > 127) {
            // add rsi, {cnt}
            writeBytes(ofs, {0x48, 0x81, 0xc6});
            writeAs<std::uint32_t>(ofs, cnt);
          } else if (cnt > 1) {
            // add rsi, {cnt}
            writeBytes(ofs, {0x48, 0x83, 0xc6});
            writeAs<std::uint8_t>(ofs, cnt);
          } else {
            // inc rsi
            writeBytes(ofs, {0x48, 0xff, 0xc6});
          }
        }
        break;
      case '<':
        {
          const auto cnt = countSuccChars(source, '<', i + 1) + 1;
          i += cnt - 1;
          if (cnt > 127) {
            // sub rsi, {cnt}
            writeBytes(ofs, {0x48, 0x81, 0xee});
            writeAs<std::uint32_t>(ofs, cnt);
          } else if (cnt > 1) {
            // sub rsi, {cnt}
            writeBytes(ofs, {0x48, 0x83, 0xee});
            writeAs<std::uint8_t>(ofs, cnt);
          } else {
            // dec rsi
            writeBytes(ofs, {0x48, 0xff, 0xce});
          }
        }
        break;
      case '+':
        {
          auto cnt = countSuccChars(source, '+', i + 1) + 1;
          i += cnt - 1;
          cnt %= 256;
          if (cnt > 1) {
            // add byte ptr [rsi], {cnt}
            writeBytes(ofs, {0x80, 0x06});
            writeAs<std::uint8_t>(ofs, cnt);
          } else if (cnt == 1) {
            // inc byte ptr [rsi]
            writeBytes(ofs, {0xfe, 0x06});
          }
        }
        break;
      case '-':
        {
          auto cnt = countSuccChars(source, '-', i + 1) + 1;
          i += cnt - 1;
          cnt %= 256;
          if (cnt > 1) {
            // sub byte ptr [rsi], {cnt}
            writeBytes(ofs, {0x80, 0x2e});
            writeAs<std::uint8_t>(ofs, cnt);
          } else if (cnt == 1) {
            // dec byte ptr [rsi]
            writeBytes(ofs, {0xfe, 0x0e});
          }
        }
        break;
      case '.':
        // mov eax, edx
        writeBytes(ofs, {0x89, 0xd0});
        // mov edi, edx
        writeBytes(ofs, {0x89, 0xd7});
        // syscall
        writeBytes(ofs, {0x0f, 0x05});
        break;
      case ',':
        // xor eax, eax
        writeBytes(ofs, {0x31, 0xc0});
        // xor edi, edi
        writeBytes(ofs, {0x31, 0xff});
        // syscall
        writeBytes(ofs, {0x0f, 0x05});
        break;
      case '[':
        // [-] または [+] はゼロ代入にする
        if (i + 2 < source.size()
            && (source[i + 1] == '+' || source[i + 1] == '-')
            && source[i + 2] == ']') {
          // mov byte ptr [rsi], dh
          writeBytes(ofs, {0x88, 0x36});
          i += 2;
        } else {
          loopStack.push(ofs.tellp());
          // cmp byte ptr [rsi], dh
          writeBytes(ofs, {0x38, 0x36});
          // je 0x********
          // ジャンプ先が決定していないので，ジャンプオフセットは後で書き込む
          // ここをジャンプオフセットの大きさに応じてshort jumpかnear jump命令を生成しようと思うと
          // 命令長が変わり実装が少し面倒になる
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
          ofs.seekp(pos + decltype(pos){4}, std::ios_base::beg);
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

  // mov eax, 0x3c
  writeAs<std::uint8_t>(ofs, 0xb8);
  writeAs<std::uint32_t>(ofs, 0x3c);
  // xor edi, edi
  writeBytes(ofs, {0x31, 0xff});
  // syscall
  writeBytes(ofs, {0x0f, 0x05});

  // Write footer
  const auto codeSize = static_cast<std::size_t>(ofs.tellp()) - kHeaderSize;
  writeFooter(ofs, codeSize);

  // Write header
  ofs.seekp(0, std::ios_base::beg);
  writeHeader(ofs, codeSize);
  ofs.seekp(0, std::ios_base::end);

  ofs.close();

  // 生成した実行ファイルに実行可能属性を付与する
#ifdef HAS_HEADER_FILESYSTEM
  std::filesystem::permissions(
    dstFilePath,
    std::filesystem::perms::owner_all
      | std::filesystem::perms::group_read | std::filesystem::perms::group_exec
      | std::filesystem::perms::others_read | std::filesystem::perms::others_exec);
#else
  ::chmod(dstFilePath, 0755);
#endif  // HAS_HEADER_FILESYSTEM

  std::system(dstFilePath);
}
