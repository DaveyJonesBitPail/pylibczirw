#include <iostream>
#include <codecvt>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <locale>
#include <string>

#include "../src/api/CZIeditAPI.h"
#include "../src/api/inc_libCzi.h"

static int test_open_close_and_read_xml(const std::wstring &path) {
  CZIeditAPI editor(path);
  if (!editor.IsOpen()) {
    std::cerr << "Editor not open\n";
    return 1;
  }
  auto xml = editor.ReadMetadataXml();
  if (xml.find("<ImageDocument") == std::string::npos) {
    std::cerr << "Root missing\n";
    return 2;
  }
  editor.Close();
  if (editor.IsOpen()) {
    std::cerr << "Editor still open\n";
    return 3;
  }
  return 0;
}

static int test_commit_title_change(const std::wstring &srcPath) {
  auto tmp = std::filesystem::temp_directory_path() / "cziedit_tmp1.czi";
  std::error_code ec;
  std::filesystem::copy_file(srcPath, tmp,
                             std::filesystem::copy_options::overwrite_existing,
                             ec);
  if (ec) {
    std::cerr << "Copy failed: " << ec.message() << "\n";
    return 10;
  }

  CZIeditAPI editor(tmp.wstring());
  auto builder = editor.CreateMetadataBuilder();
  libCZI::GeneralDocumentInfo gdi;
  gdi.SetTitle(L"NativeCommitted");
  builder->SetGeneralDocumentInfo(gdi);
  if (!builder->CanCommit()) {
    std::cerr << "CanCommit false\n";
    return 11;
  }
  builder->Commit();

  CZIeditAPI editor2(tmp.wstring());
  auto xml = editor2.ReadMetadataXml();
  if (xml.find("<Title>NativeCommitted</Title>") == std::string::npos) {
    std::cerr << "Title not committed\n";
    return 12;
  }
  return 0;
}

static int test_channel_info(const std::wstring &srcPath) {
  auto tmp = std::filesystem::temp_directory_path() / "cziedit_tmp2.czi";
  std::error_code ec;
  std::filesystem::copy_file(srcPath, tmp,
                             std::filesystem::copy_options::overwrite_existing,
                             ec);
  if (ec) {
    std::cerr << "Copy failed: " << ec.message() << "\n";
    return 10;
  }

  CZIeditAPI editor(tmp.wstring());
  auto builder = editor.CreateMetadataBuilder();
  auto displaySettings = editor.ReadDisplaySettings();
  std::cout << "Original display settings:\n";
  std::cout << "Channel count: " << displaySettings.size() << "\n";
  for (const auto &[idx, settings] : displaySettings) {
    std::wcout << "Channel " << idx << ": name='" << settings.name
               << "', description='" << settings.description << "'\n";
  }

  return 0;
}

// Not meant as a verification test but used for debugging!
static int test_custom_attribute(const std::wstring &srcPath) {
  auto tmp = std::filesystem::temp_directory_path() / "cziedit_tmp2.czi";
  std::error_code ec;
  std::filesystem::copy_file(srcPath, tmp,
                             std::filesystem::copy_options::overwrite_existing,
                             ec);
  if (ec) {
    std::cerr << "Copy failed: " << ec.message() << "\n";
    return 10;
  }

  CZIeditAPI editor(srcPath);

  auto builder = editor.CreateMetadataBuilder();
  auto customValue = editor.ReadCustomKeyValue("Blah");
  std::cout << "Original custom value for 'Blah': type="
            << static_cast<int>(customValue.GetType()) << "\n";
  builder->SetCustomKeyValue("Blah", libCZI::CustomValueVariant("BlahBlah"));
  builder->Commit();
  std::cout << editor.ReadMetadataXml() << std::endl;
  auto setVar = editor.ReadCustomKeyValue("Blah");
  std::cout << "After setting, custom value for 'Blah': type="
            << static_cast<int>(setVar.GetType()) << setVar.GetAsStringOrThrow()
            << "\n";
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: cziedit_tests image.czi\n";
    return 100;
  }
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  const std::wstring path = converter.from_bytes(argv[1]);

  int rc = 0;
  rc |= test_open_close_and_read_xml(path);
  rc |= test_commit_title_change(path);
  rc |= test_channel_info(path);
  rc |= test_custom_attribute(path);
  if (rc != 0) std::cerr << "One or more editor tests failed.\n";
  return rc;
}
