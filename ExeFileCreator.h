#ifndef EXE_FILE_CREATOR_H_
#define EXE_FILE_CREATOR_H_

#include <windows.h>
#include <vector>
#include <string>
#include <map>

class ExeFileCreator {
 public:
  ExeFileCreator();
  ~ExeFileCreator();

  unsigned int AddString(const char* const string);
  int DeleteString(const char* const string);
  
  unsigned int AddImportFunction(const char* const dllName, const char* const functionName);
  int DeleteImportFunction(const char* const dllName, const char* const functionName);
  
  //�R�[�h�T�C�Y��SECTION_SIZE���z���Ă�����A�󂯕t���Ȃ�
  int SetCode(std::string codeString);
  int Create(const char* const filename) const;
  
 private:
  std::map<std::string, unsigned int> resourceInformation; //first=�����񃊃\�[�X, second=�i�[�����A�h���X
  std::map<unsigned int, unsigned int> resourceAddressPool; //first=�󔒋�ԃT�C�Y, second=�󔒋�ԃA�h���X
  int lastResourceAddress;

  typedef std::map<std::string, unsigned int> FunctionInformation; //first=�֐���, second=�i�[�����A�h���X
  std::map<std::string, FunctionInformation> importInformation; //first=DLL��, second=�֐����(���)���X�g
  std::vector<unsigned int> importFunctionAddressPool;
  int lastImportFunctionAddress;
  
  std::vector<BYTE> code;
  
  static const int SECTION_SIZE = 0x1000;
  static const int HEADER_BASE = 0;
  static const int CODE_BASE = HEADER_BASE + SECTION_SIZE;
  static const int IMPORT_BASE = CODE_BASE + SECTION_SIZE;
  static const int RESOURCE_BASE = IMPORT_BASE + SECTION_SIZE;
  
  static const int IMAGE_BASE = 0x00400000;
  
  //DWORD�^��x���o�C�g���Ƃɋ�؂�A���g���G���f�B�A����b0�`b3�Ɋi�[����
  void DWORDtoBYTE(BYTE& b0, BYTE& b1, BYTE& b2, BYTE& b3, DWORD x) const;
};

#endif