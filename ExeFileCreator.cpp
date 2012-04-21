#include <windows.h>
#include <memory.h>
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include "ExeFileCreator.h"

ExeFileCreator::ExeFileCreator():lastResourceAddress(IMAGE_BASE + RESOURCE_BASE),
                                 lastImportFunctionAddress(IMAGE_BASE + CODE_BASE + 5) {

}

ExeFileCreator::~ExeFileCreator() {

}

unsigned int ExeFileCreator::AddString(const char* const string) {
  unsigned int retVal = 0;
  if(resourceAddressPool.empty()) {
    retVal = lastResourceAddress;
    resourceInformation.insert(std::make_pair(string, lastResourceAddress));
    lastResourceAddress += strlen(string) + 1;
  }else {
    std::map<unsigned int, unsigned int>::iterator it, end;
    it = resourceAddressPool.begin();
    end = resourceAddressPool.end();
    bool notfound = true;
    while(it != end) {
      if(it->first > strlen(string)) {
        notfound = false;
        retVal = it->second;
        resourceInformation.insert(std::make_pair(string, it->second));
        resourceAddressPool.erase(it);
        break;
      }
      ++it;
    }
    if(notfound) {
      retVal = lastResourceAddress;
      resourceInformation.insert(std::make_pair(string, lastResourceAddress));
      lastResourceAddress += strlen(string) + 1;
    }
  }
  return retVal;
}

int ExeFileCreator::DeleteString(const char* const string) {
  std::map<std::string, unsigned int>::iterator it;
  it = resourceInformation.find(string);
  if(it != resourceInformation.end()) {
    resourceAddressPool.insert(std::make_pair(it->first.size() + 1, it->second));
    resourceInformation.erase(it);
    return 0;
  }else {
    return 1;
  }
}

unsigned int ExeFileCreator::AddImportFunction(const char* const dllName, const char* const functionName) {
  unsigned int retVal = 0;
  std::map<std::string, FunctionInformation>::iterator it;
  it = importInformation.find(dllName);
  if(it == importInformation.end()) {
    importInformation.insert(std::make_pair(dllName, FunctionInformation()));
    it = importInformation.find(dllName);
  }
  if(importFunctionAddressPool.empty()) {
    retVal = lastImportFunctionAddress;
    it->second.insert(std::make_pair(functionName, lastImportFunctionAddress));
    lastImportFunctionAddress += 4;
  }else {
    std::vector<unsigned int>::iterator poolIt;
    poolIt = importFunctionAddressPool.begin();
    retVal = *poolIt;
    it->second.insert(std::make_pair(functionName, *poolIt));
    importFunctionAddressPool.erase(poolIt);
  }
  return retVal;
}

int ExeFileCreator::DeleteImportFunction(const char* const dllName, const char* const functionName) {
  std::map<std::string, FunctionInformation>::iterator it;
  it = importInformation.find(dllName);
  if(it != importInformation.end()) {
    FunctionInformation::iterator funcIt;
    funcIt = it->second.find(functionName);
    if(funcIt != it->second.end()) {
      importFunctionAddressPool.push_back(funcIt->second);
      it->second.erase(funcIt);
      return 0;
    }
  }
  return 1;
}

int ExeFileCreator::SetCode(std::string codeString) {
  std::string::iterator it, end;
  BYTE temp = 0;
  int ten = 0;
  code.clear();
  it = codeString.begin();
  end = codeString.end();
  while(it != end) {
    if(*it >= '0' && *it <= '9') {
      temp += *it - '0' + 0x0;
    }else if(*it >= 'a' && *it <= 'f') {
      temp += *it - 'a' + 0xa;
    }else if(*it >= 'A' && *it <= 'F') {
      temp += *it - 'A' + 0xA;
    }else {
      ++it;
      continue;
    }
    ten = 1 - ten;
    if(ten == 0) {
      code.push_back(temp);
      temp = 0;
    }else {
      temp *= 0x10;
    }
    ++it;
  }
  if(ten == 1) {
    code.push_back(temp);
  }
  if(static_cast<int>(code.size()) > SECTION_SIZE) {
    code.clear();
    return 1;
  }
  return 0;
}
  
int ExeFileCreator::Create(const char* const filename) const {

  //_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
  //                                              _/
  //  EXE�����̏����A���\�[�X�f�[�^�̍č\�z       _/
  //                                              _/
  //_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
  
  std::vector<BYTE> resourceData;
  std::map<std::string, unsigned int>::const_iterator resInfoIt, resInfoEnd;
  resInfoIt = resourceInformation.begin();
  resInfoEnd = resourceInformation.end();
  while(resInfoIt != resInfoEnd) {
    for(unsigned int i = resourceData.size();i < resInfoIt->second - (IMAGE_BASE + RESOURCE_BASE) + resInfoIt->first.size() + 1;++i) {
      resourceData.push_back(0);
    }
    for(unsigned int offset = 0;offset < resInfoIt->first.size();++offset) {
      resourceData[resInfoIt->second - (IMAGE_BASE + RESOURCE_BASE) + offset] = resInfoIt->first[offset];
    }
    ++resInfoIt;
  }
  
  

  //_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
  //                                              _/
  //  �C���|�[�g�f�X�N���v�^�̍\�z                _/
  //                                              _/
  //_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
  
  std::vector<IMAGE_IMPORT_DESCRIPTOR> importDescList;
  std::vector<BYTE> IAT; //�C���|�[�g�A�h���X�e�[�u��
  std::vector<BYTE> INT; //�C���|�[�g�l�[���e�[�u��
  std::vector<char> stringTable; //DLL����֐������i�[�����
  BYTE jmpEntryPointInstruction[5] = {0xE9, 0xFF, 0xFF, 0xFF, 0xFF}; // �R�[�h�̎n�߂ɃW�����v���閽�߂��i�[�����
  std::vector<BYTE> functionAddressList; //API�Ăяo�����߂ւ̑��΃A�h���X�̃��X�g
  std::vector<BYTE> jmpFunctionInstruction; //�C���|�[�g���ꂽAPI���Ăяo�����ߌQ���i�[�����
  
  //�ŏ���importDescList�AIAT�AINT�AfunctionAddressList�̃T�C�Y���m�肳���Ă���
  std::map<std::string, FunctionInformation>::const_iterator impit, impend;
  impit = importInformation.begin();
  impend = importInformation.end();
  while(impit != impend) {
    importDescList.push_back(IMAGE_IMPORT_DESCRIPTOR());
    FunctionInformation::const_iterator funcit, funcend;
    funcit = impit->second.begin();
    funcend = impit->second.end();
    while(funcit != funcend) {
      for(int i = 0;i < 4;++i) {
        IAT.push_back(0);
        INT.push_back(0);
      }
      for(unsigned int i = functionAddressList.size();i < funcit->second + 4 - (IMAGE_BASE + CODE_BASE + 5);++i) {
        functionAddressList.push_back(0);
      }
      ++funcit;
    }
    for(int i = 0;i < 4;++i) {
      IAT.push_back(0);
      INT.push_back(0);
    }
    ++impit;
  }
  importDescList.push_back(IMAGE_IMPORT_DESCRIPTOR());
  
  //�C���|�[�g�f�X�N���v�^�AIAT�AINT�AstringTable�AfunctionAddressList�AjmpFunctionInstruction�A���ꂼ��̊J�n�A�h���X���v�Z
  int startAddressImportDescList = IMPORT_BASE;
  int startAddressIAT = startAddressImportDescList + importDescList.size() * sizeof(IMAGE_IMPORT_DESCRIPTOR);
  int startAddressINT = startAddressIAT + IAT.size();
  int startAddressStringTable = startAddressINT + INT.size();
  int startAddressFunctionAddressList = IMAGE_BASE + CODE_BASE + 5;
  
  //�C���|�[�g�e�[�u���AIAT�AINT�AstringTable�AfunctionAddressList�AjmpFunctionInstruction�̍\�z
  int pointerOfImportDescList = 0;
  int pointerOfIAT = 0;
  int pointerOfINT = 0;
  int pointerOfStringTable = 0;
  int pointerOfJmpFunctionInstruction = 0;
  impit = importInformation.begin();
  impend = importInformation.end();
  while(impit != impend) {
    //�C���|�[�g�f�X�N���v�^�̐ݒ�
    importDescList[pointerOfImportDescList].TimeDateStamp = 0;
    importDescList[pointerOfImportDescList].ForwarderChain = 0;
    importDescList[pointerOfImportDescList].Name = startAddressStringTable + pointerOfStringTable;
    importDescList[pointerOfImportDescList].OriginalFirstThunk = startAddressINT + pointerOfINT;
    importDescList[pointerOfImportDescList].FirstThunk = startAddressIAT + pointerOfIAT;
    ++pointerOfImportDescList;

    //stringTable��DLL����ǉ�
    std::string::const_iterator strit, strend;
    strit = impit->first.begin();
    strend = impit->first.end();
    while(strit != strend) {
      stringTable.push_back(*strit);
      ++pointerOfStringTable;
      ++strit;
    }
    stringTable.push_back('\0');
    ++pointerOfStringTable;
    
    //IAT�AINT������
    FunctionInformation::const_iterator funcit, funcend;
    funcit = impit->second.begin();
    funcend = impit->second.end();
    while(funcit != funcend) {
      //functionAddressList�Ɋ֐��Ăяo�����߂ւ̑��΃A�h���X��ݒ�
      int writePos = funcit->second - startAddressFunctionAddressList;
      DWORDtoBYTE(functionAddressList[writePos + 0], functionAddressList[writePos + 1], functionAddressList[writePos + 2], functionAddressList[writePos + 3], startAddressFunctionAddressList + functionAddressList.size() + code.size() + pointerOfJmpFunctionInstruction);

      //jmpFunctionInstruction�ɃC���|�[�g�֐��փW�����v���閽�߂�ݒ�
      jmpFunctionInstruction.push_back(0xFF);
      jmpFunctionInstruction.push_back(0x25);
      for(int i = 0;i < 4;++i) jmpFunctionInstruction.push_back(0);
      DWORDtoBYTE(jmpFunctionInstruction[pointerOfJmpFunctionInstruction + 2], jmpFunctionInstruction[pointerOfJmpFunctionInstruction + 3], jmpFunctionInstruction[pointerOfJmpFunctionInstruction + 4], jmpFunctionInstruction[pointerOfJmpFunctionInstruction + 5], IMAGE_BASE + startAddressIAT + pointerOfIAT);
      pointerOfJmpFunctionInstruction += 6;

      //IAT�AINT�Ɋ֐����ւ̃A�h���X��ݒ�
      DWORDtoBYTE(IAT[pointerOfIAT + 0], IAT[pointerOfIAT + 1], IAT[pointerOfIAT + 2], IAT[pointerOfIAT + 3], startAddressStringTable + pointerOfStringTable);
      pointerOfIAT += 4;
      DWORDtoBYTE(INT[pointerOfINT + 0], INT[pointerOfINT + 1], INT[pointerOfINT + 2], INT[pointerOfINT + 3], startAddressStringTable + pointerOfStringTable);
      pointerOfINT += 4;
      
      //stringTable�Ɋ֐�����ǉ�
      stringTable.push_back('\0');
      stringTable.push_back('\0');
      pointerOfStringTable += 2;
      std::string::const_iterator strit, strend;
      strit = funcit->first.begin();
      strend = funcit->first.end();
      while(strit != strend) {
        stringTable.push_back(*strit);
        ++pointerOfStringTable;
        ++strit;
      }
      stringTable.push_back('\0');
      ++pointerOfStringTable;
      
      ++funcit;
    }
    //IAT�AINT�̍Ō��NULL��ݒ�
    DWORDtoBYTE(IAT[pointerOfIAT + 0], IAT[pointerOfIAT + 1], IAT[pointerOfIAT + 2], IAT[pointerOfIAT + 3], 0);
    pointerOfIAT += 4;
    DWORDtoBYTE(INT[pointerOfINT + 0], INT[pointerOfINT + 1], INT[pointerOfINT + 2], INT[pointerOfINT + 3], 0);
    pointerOfINT += 4;
    
    ++impit;
  }
  
  //jmpEntryPointInstruction�̐ݒ�
  DWORDtoBYTE(jmpEntryPointInstruction[1], jmpEntryPointInstruction[2], jmpEntryPointInstruction[3], jmpEntryPointInstruction[4], functionAddressList.size());
  
  
  

  //_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
  //                                              _/
  //  EXE�t�@�C���̃w�b�_�̐ݒ�                   _/
  //                                              _/
  //_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
  
  //DOS�w�b�_�ݒ�
  IMAGE_DOS_HEADER imageDosHeader;
  imageDosHeader.e_magic=   0x5A4D;
  imageDosHeader.e_cblp=    0x0090;
  imageDosHeader.e_cp=      0x0003;
  imageDosHeader.e_crlc=    0;
  imageDosHeader.e_cparhdr= 4;
  imageDosHeader.e_minalloc=0x0000;
  imageDosHeader.e_maxalloc=0xFFFF;
  imageDosHeader.e_ss=      0x0000;
  imageDosHeader.e_sp=      0x00B8;
  imageDosHeader.e_csum=    0x0000;
  imageDosHeader.e_ip=      0x0000;
  imageDosHeader.e_cs=      0x0000;
  imageDosHeader.e_lfarlc=  0x0040;
  imageDosHeader.e_ovno=    0x0000;
  imageDosHeader.e_res[0]=  0;
  imageDosHeader.e_res[1]=  0;
  imageDosHeader.e_res[2]=  0;
  imageDosHeader.e_res[3]=  0;
  imageDosHeader.e_oemid=   0x0000;
  imageDosHeader.e_oeminfo= 0x0000;
  imageDosHeader.e_res2[0]= 0;
  imageDosHeader.e_res2[1]= 0;
  imageDosHeader.e_res2[2]= 0;
  imageDosHeader.e_res2[3]= 0;
  imageDosHeader.e_res2[4]= 0;
  imageDosHeader.e_res2[5]= 0;
  imageDosHeader.e_res2[6]= 0;
  imageDosHeader.e_res2[7]= 0;
  imageDosHeader.e_res2[8]= 0;
  imageDosHeader.e_res2[9]= 0;
  imageDosHeader.e_lfanew=  0x0100; //PE�w�b�_�̈ʒu


  // PE�w�b�_�ݒ�
  IMAGE_NT_HEADERS imagePeHeader;
  imagePeHeader.Signature                                  = IMAGE_NT_SIGNATURE;
  imagePeHeader.FileHeader.Machine                         = IMAGE_FILE_MACHINE_I386;
  imagePeHeader.FileHeader.NumberOfSections                = 3;
  imagePeHeader.FileHeader.TimeDateStamp                   = static_cast<DWORD>(std::time(0));
  imagePeHeader.FileHeader.PointerToSymbolTable            = 0x00000000;
  imagePeHeader.FileHeader.NumberOfSymbols                 = 0x00000000;
  imagePeHeader.FileHeader.SizeOfOptionalHeader            = 224; //IMAGE_SIZEOF_NT_OPTIONAL32_HEADER
  imagePeHeader.FileHeader.Characteristics                 = IMAGE_FILE_EXECUTABLE_IMAGE |
                                                          IMAGE_FILE_32BIT_MACHINE |
                                                          IMAGE_FILE_LINE_NUMS_STRIPPED |
                                                          IMAGE_FILE_LOCAL_SYMS_STRIPPED;
  imagePeHeader.OptionalHeader.Magic                       = 0x010B;
  imagePeHeader.OptionalHeader.MajorLinkerVersion          = 1;
  imagePeHeader.OptionalHeader.MinorLinkerVersion          = 0;
  imagePeHeader.OptionalHeader.SizeOfCode                  = static_cast<DWORD>(5 + functionAddressList.size() + code.size() + jmpFunctionInstruction.size());
  imagePeHeader.OptionalHeader.SizeOfInitializedData       = SECTION_SIZE;
  imagePeHeader.OptionalHeader.SizeOfUninitializedData     = 0;
  imagePeHeader.OptionalHeader.AddressOfEntryPoint         = CODE_BASE;
  imagePeHeader.OptionalHeader.BaseOfCode                  = CODE_BASE;
  imagePeHeader.OptionalHeader.BaseOfData                  = RESOURCE_BASE;
  imagePeHeader.OptionalHeader.ImageBase                   = IMAGE_BASE;
  imagePeHeader.OptionalHeader.SectionAlignment            = SECTION_SIZE;
  imagePeHeader.OptionalHeader.FileAlignment               = SECTION_SIZE;
  imagePeHeader.OptionalHeader.MajorOperatingSystemVersion = 4;
  imagePeHeader.OptionalHeader.MinorOperatingSystemVersion = 0;
  imagePeHeader.OptionalHeader.MajorImageVersion           = 0;
  imagePeHeader.OptionalHeader.MinorImageVersion           = 0;
  imagePeHeader.OptionalHeader.MajorSubsystemVersion       = 4;
  imagePeHeader.OptionalHeader.MinorSubsystemVersion       = 0;
  imagePeHeader.OptionalHeader.Win32VersionValue           = 0;
  imagePeHeader.OptionalHeader.SizeOfImage                 = SECTION_SIZE * 4;
  imagePeHeader.OptionalHeader.SizeOfHeaders               = SECTION_SIZE;
  imagePeHeader.OptionalHeader.CheckSum                    = 0;
  imagePeHeader.OptionalHeader.Subsystem                   = IMAGE_SUBSYSTEM_WINDOWS_CUI;
  imagePeHeader.OptionalHeader.DllCharacteristics          = 0;
  imagePeHeader.OptionalHeader.SizeOfStackReserve          = 0x00100000;
  imagePeHeader.OptionalHeader.SizeOfStackCommit           = 0x00001000;
  imagePeHeader.OptionalHeader.SizeOfHeapReserve           = 0x00100000;
  imagePeHeader.OptionalHeader.SizeOfHeapCommit            = 0x00001000;
  imagePeHeader.OptionalHeader.LoaderFlags                 = 0;
  imagePeHeader.OptionalHeader.NumberOfRvaAndSizes         = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

  imagePeHeader.OptionalHeader.DataDirectory[0].VirtualAddress = 0;
  imagePeHeader.OptionalHeader.DataDirectory[0].Size = 0;
  imagePeHeader.OptionalHeader.DataDirectory[1].VirtualAddress = IMPORT_BASE;
  imagePeHeader.OptionalHeader.DataDirectory[1].Size = SECTION_SIZE;
  imagePeHeader.OptionalHeader.DataDirectory[2].VirtualAddress = 0;
  imagePeHeader.OptionalHeader.DataDirectory[2].Size = 0;
  imagePeHeader.OptionalHeader.DataDirectory[3].VirtualAddress = 0;
  imagePeHeader.OptionalHeader.DataDirectory[3].Size = 0;
  imagePeHeader.OptionalHeader.DataDirectory[4].VirtualAddress = 0;
  imagePeHeader.OptionalHeader.DataDirectory[4].Size = 0;
  imagePeHeader.OptionalHeader.DataDirectory[5].VirtualAddress = 0;
  imagePeHeader.OptionalHeader.DataDirectory[5].Size = 0;
  imagePeHeader.OptionalHeader.DataDirectory[6].VirtualAddress = 0;
  imagePeHeader.OptionalHeader.DataDirectory[6].Size = 0;
  imagePeHeader.OptionalHeader.DataDirectory[7].VirtualAddress = 0;
  imagePeHeader.OptionalHeader.DataDirectory[7].Size = 0;
  imagePeHeader.OptionalHeader.DataDirectory[8].VirtualAddress = 0;
  imagePeHeader.OptionalHeader.DataDirectory[8].Size = 0;
  imagePeHeader.OptionalHeader.DataDirectory[9].VirtualAddress = 0;
  imagePeHeader.OptionalHeader.DataDirectory[9].Size = 0;
  imagePeHeader.OptionalHeader.DataDirectory[10].VirtualAddress = 0;
  imagePeHeader.OptionalHeader.DataDirectory[10].Size = 0;
  imagePeHeader.OptionalHeader.DataDirectory[11].VirtualAddress = 0;
  imagePeHeader.OptionalHeader.DataDirectory[11].Size = 0;
  imagePeHeader.OptionalHeader.DataDirectory[12].VirtualAddress = startAddressIAT;
  imagePeHeader.OptionalHeader.DataDirectory[12].Size = pointerOfIAT;
  imagePeHeader.OptionalHeader.DataDirectory[13].VirtualAddress = 0;
  imagePeHeader.OptionalHeader.DataDirectory[13].Size = 0;
  imagePeHeader.OptionalHeader.DataDirectory[14].VirtualAddress = 0;
  imagePeHeader.OptionalHeader.DataDirectory[14].Size = 0;
  imagePeHeader.OptionalHeader.DataDirectory[15].VirtualAddress = 0;
  imagePeHeader.OptionalHeader.DataDirectory[15].Size = 0;
  
  //�R�[�h�Z�N�V�����w�b�_�̐ݒ�
  IMAGE_SECTION_HEADER codeSectionHeader;
  memset(reinterpret_cast<char*>(codeSectionHeader.Name), 0, IMAGE_SIZEOF_SHORT_NAME);
  lstrcpy(reinterpret_cast<char*>(codeSectionHeader.Name), ".text");
  codeSectionHeader.Misc.VirtualSize     = SECTION_SIZE; //��������̃T�C�Y
  codeSectionHeader.VirtualAddress       = CODE_BASE; //��������̊J�n�A�h���X
  //�t�@�C����̃T�C�Y
  codeSectionHeader.SizeOfRawData        = static_cast<DWORD>(5 + functionAddressList.size() + code.size() + jmpFunctionInstruction.size());
  codeSectionHeader.PointerToRawData     = CODE_BASE; //�t�@�C����̊J�n�A�h���X
  codeSectionHeader.PointerToRelocations = 0;
  codeSectionHeader.PointerToLinenumbers = 0;
  codeSectionHeader.NumberOfRelocations  = 0;
  codeSectionHeader.NumberOfLinenumbers  = 0;
  codeSectionHeader.Characteristics      = IMAGE_SCN_MEM_EXECUTE |
                                           IMAGE_SCN_MEM_READ |
                                           IMAGE_SCN_CNT_CODE;
  //�C���|�[�g�Z�N�V�����w�b�_�̐ݒ�
  IMAGE_SECTION_HEADER importSectionHeader;
  memset(reinterpret_cast<char*>(importSectionHeader.Name), 0, IMAGE_SIZEOF_SHORT_NAME);
  lstrcpy(reinterpret_cast<char*>(importSectionHeader.Name), ".idata");
  importSectionHeader.Misc.VirtualSize     = SECTION_SIZE; //��������̃T�C�Y
  importSectionHeader.VirtualAddress       = IMPORT_BASE; //��������̊J�n�A�h���X
  importSectionHeader.SizeOfRawData        = SECTION_SIZE; //�t�@�C����̃T�C�Y
  importSectionHeader.PointerToRawData     = IMPORT_BASE; //�t�@�C����̊J�n�A�h���X
  importSectionHeader.PointerToRelocations = 0;
  importSectionHeader.PointerToLinenumbers = 0;
  importSectionHeader.NumberOfRelocations  = 0;
  importSectionHeader.NumberOfLinenumbers  = 0;
  importSectionHeader.Characteristics      = IMAGE_SCN_CNT_INITIALIZED_DATA |
                                             IMAGE_SCN_MEM_READ;
  //�f�[�^�Z�N�V�����w�b�_�̐ݒ�
  IMAGE_SECTION_HEADER dataSectionHeader;
  memset(reinterpret_cast<char*>(dataSectionHeader.Name), 0, IMAGE_SIZEOF_SHORT_NAME);
  lstrcpy(reinterpret_cast<char*>(dataSectionHeader.Name), ".sdata");
  dataSectionHeader.Misc.VirtualSize     = SECTION_SIZE;  //��������̃T�C�Y
  dataSectionHeader.VirtualAddress       = RESOURCE_BASE;  //��������̊J�n�A�h���X
  dataSectionHeader.SizeOfRawData        = SECTION_SIZE;  //�t�@�C����̃T�C�Y
  dataSectionHeader.PointerToRawData     = RESOURCE_BASE;  //�t�@�C����̊J�n�A�h���X
  dataSectionHeader.PointerToRelocations = 0;
  dataSectionHeader.PointerToLinenumbers = 0;
  dataSectionHeader.NumberOfRelocations  = 0;
  dataSectionHeader.NumberOfLinenumbers  = 0;
  dataSectionHeader.Characteristics      = IMAGE_SCN_CNT_INITIALIZED_DATA |
                                           IMAGE_SCN_MEM_READ |
                                           IMAGE_SCN_MEM_WRITE;
                                           
                                           

  //_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
  //                                              _/
  //  EXE�t�@�C���̏o��                           _/
  //                                              _/
  //_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
  HANDLE outputFileHandle;
  DWORD accessBytes;
  const char nullSpace[SECTION_SIZE] = {0};

  outputFileHandle = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,NULL);
  if(outputFileHandle == INVALID_HANDLE_VALUE) {
    MessageBoxA(NULL,"�t�@�C���̍쐬�Ɏ��s���܂���","Error",MB_OK);
    return 1;
  }

  //�w�b�_
  int fileOffset;
  WriteFile(outputFileHandle, static_cast<void *>(&imageDosHeader), sizeof(IMAGE_DOS_HEADER), &accessBytes, NULL);
  fileOffset = accessBytes;

  //0x0100�܂�NULL����ׂ�
  WriteFile(outputFileHandle, nullSpace, 0x0100 - fileOffset, &accessBytes, NULL);
  fileOffset += accessBytes;

  //PE�w�b�_
  WriteFile(outputFileHandle, &imagePeHeader, sizeof(IMAGE_NT_HEADERS), &accessBytes, NULL);
  fileOffset += accessBytes;

  //�R�[�h�Z�N�V�����w�b�_
  WriteFile(outputFileHandle, &codeSectionHeader, sizeof(IMAGE_SECTION_HEADER), &accessBytes, NULL);
  fileOffset += accessBytes;

  //�C���|�[�g�Z�N�V���� �w�b�_
  WriteFile(outputFileHandle, &importSectionHeader, sizeof(IMAGE_SECTION_HEADER), &accessBytes, NULL);
  fileOffset += accessBytes;

  //�f�[�^�Z�N�V�����w�b�_
  WriteFile(outputFileHandle, &dataSectionHeader, sizeof(IMAGE_SECTION_HEADER), &accessBytes, NULL);
  fileOffset += accessBytes;

  //SECTION_SIZE�܂�NULL����ׂ�
  WriteFile(outputFileHandle, nullSpace, SECTION_SIZE - fileOffset, &accessBytes, NULL);
  fileOffset += accessBytes;

  //�R�[�h���o��
  WriteFile(outputFileHandle, jmpEntryPointInstruction, sizeof(jmpEntryPointInstruction), &accessBytes, NULL);
  fileOffset += accessBytes;
  
  std::vector<BYTE>::const_iterator codeit, codeend;
  codeit = functionAddressList.begin();
  codeend = functionAddressList.end();
  while(codeit != codeend)
  {
    WriteFile(outputFileHandle, &(*codeit), sizeof(*codeit), &accessBytes, NULL);
    fileOffset += accessBytes;

    ++codeit;
  }
  codeit = code.begin();
  codeend = code.end();
  while(codeit != codeend)
  {
    WriteFile(outputFileHandle, &(*codeit), sizeof(*codeit), &accessBytes, NULL);
    fileOffset += accessBytes;

    ++codeit;
  }
  codeit = jmpFunctionInstruction.begin();
  codeend = jmpFunctionInstruction.end();
  while(codeit != codeend)
  {
    WriteFile(outputFileHandle, &(*codeit), sizeof(*codeit), &accessBytes, NULL);
    fileOffset += accessBytes;

    ++codeit;
  }

  //IMAGE_BASE�܂�NULL����ׂ�
  WriteFile(outputFileHandle, nullSpace, IMPORT_BASE - fileOffset, &accessBytes, NULL);
  fileOffset += accessBytes;

  //�C���|�[�g�f�B���N�g���e�[�u��
  std::vector<IMAGE_IMPORT_DESCRIPTOR>::const_iterator importDescIt, importDescEnd;
  importDescIt = importDescList.begin();
  importDescEnd = importDescList.end();
  while(importDescIt != importDescEnd)
  {
    WriteFile(outputFileHandle, &(*importDescIt), sizeof(IMAGE_IMPORT_DESCRIPTOR), &accessBytes, NULL);
    fileOffset += accessBytes;

    ++importDescIt;
  }

  //startAddressIAT�܂�NULL����ׂ�
  WriteFile(outputFileHandle, nullSpace, startAddressIAT - fileOffset, &accessBytes, NULL);
  fileOffset += accessBytes;

  //IAT
  std::vector<BYTE>::const_iterator iatit, iatend;
  iatit = IAT.begin();
  iatend = IAT.end();
  while(iatit != iatend) {
    WriteFile(outputFileHandle, &(*iatit), sizeof(*iatit), &accessBytes, NULL);
    fileOffset += accessBytes;
    
    ++iatit;
  }

  //startAddressINT�܂�NULL����ׂ�
  WriteFile(outputFileHandle, nullSpace, startAddressINT - fileOffset, &accessBytes, NULL);
  fileOffset += accessBytes;

  //INT
  std::vector<BYTE>::const_iterator intit, intend;
  intit = INT.begin();
  intend = INT.end();
  while(intit != intend) {
    WriteFile(outputFileHandle, &(*intit), sizeof(*intit), &accessBytes, NULL);
    fileOffset += accessBytes;
    
    ++intit;
  }

  //startAddressStringTable�܂�NULL����ׂ�
  WriteFile(outputFileHandle, nullSpace, startAddressStringTable - fileOffset, &accessBytes, NULL);
  fileOffset += accessBytes;

  //stringTable
  std::vector<char>::const_iterator strit, strend;
  strit = stringTable.begin();
  strend = stringTable.end();
  while(strit != strend) {
    WriteFile(outputFileHandle, &(*strit), sizeof(*strit), &accessBytes, NULL);
    fileOffset += accessBytes;
    
    ++strit;
  }
  
  //RESOURCE_BASE�܂�NULL����ׂ�
  WriteFile(outputFileHandle, nullSpace, RESOURCE_BASE - fileOffset, &accessBytes, NULL);
  fileOffset += accessBytes;

  //�f�[�^
  std::vector<BYTE>::iterator resit, resend;
  resit = resourceData.begin();
  resend = resourceData.end();
  while(resit != resend)
  {
    WriteFile(outputFileHandle, &(*resit), sizeof(*resit), &accessBytes, NULL);
    fileOffset += accessBytes;

    ++resit;
  }

  //SECTION_SIZE�܂�NULL
  if(fileOffset % SECTION_SIZE)
  {
    WriteFile(outputFileHandle, nullSpace, SECTION_SIZE - fileOffset % SECTION_SIZE, &accessBytes, NULL);
    fileOffset += accessBytes;
  }

  //�������ݏI��
  CloseHandle(outputFileHandle);

  //MessageBoxA(NULL, "EXE�t�@�C���̐����������Ɋ������܂����B", "Success!", MB_OK);

  return 0;
}

inline void ExeFileCreator::DWORDtoBYTE(BYTE& b0, BYTE& b1, BYTE& b2, BYTE& b3, DWORD x) const {
  b3 = static_cast<BYTE>(x >> 24);
  x %= (2<<23);
  b2 = static_cast<BYTE>(x >> 16);
  x %= (2<<15);
  b1 = static_cast<BYTE>(x >> 8);
  x %= (2<<7);
  b0 = static_cast<BYTE>(x);
}