//===- Parser.hpp ---------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
//     limitations under the License.
//
//===----------------------------------------------------------------===//

#pragma once

#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

using llvm::StringRef;

enum class StatusGroup : uint8_t {
  SUCCESS   = 0x0,    // 0x0NNN...
  INFO      = 0x4,    // 0x4NNN...
  WARNING   = 0x8,    // 0x8NNN...
  ERROR     = 0xC,    // 0xCNNN...
};

enum class Subgroup : uint16_t {
  STATUS    = 0x000,  // 0xN0000...
  WOW       = 0x009,  // 0xN0009...
  INVALID   = 0x00A,  // 0xN000A...
  DBG       = 0x010,  // 0xN0010...*
  RPCA      = 0x020,  // 0xN0020...*
  RPCB      = 0x030,  // 0xN0030...*
  PNP       = 0x040,  // 0xN0040...
  CTX       = 0x0A0,  // 0xN00A0...
  MUI       = 0x0B0,  // 0xN00B0...
  CLUSTER   = 0x130,  // 0xN0130...
  ACPI      = 0x140,  // 0xN0140...
  FLT       = 0x1C0,  // 0xN01C0...
  SXS       = 0x150,  // 0xN0150...
  RECOVERY  = 0x190,  // 0xN0190...
  LOG       = 0x1A0,  // 0xN01A0...
  VIDEO     = 0x1B0,  // 0xN01B0...
  MONITOR   = 0x1D0,  // 0xN01D0...
  GRAPHICS  = 0x1E0,  // 0xN01E0...
  FVE       = 0x210,  // 0xN0210...
  FWP       = 0x220,  // 0xN0220...
  NDISA     = 0x230,  // 0xN0230...
  NDISB     = 0x231,  // 0xN0231...
  NDISC     = 0x232,  // 0xN0232...
  IPSECA    = 0x360,  // 0xN0360...
  IPSECB    = 0x368,  // 0xN0368...
  VOLMGR    = 0x380,  // 0xN0380...
  VIRTDISK  = 0x3A0,  // 0xN03A0...
  // Meta groups:
  RPC       = 0xF00,
  NDIS      = 0xF01,
  IPSEC     = 0xF02,
};

struct NtStatus {
  uint32_t  Code;
  Subgroup  SG;
  StringRef Name;
  StringRef Message;
};

struct StatusCtx {
  StatusGroup Group;
  Subgroup SG;
};

struct NtCodeParser {
  using CodePair = std::pair<StatusGroup, NtStatus>;
  using StatusGroupVec = llvm::SmallVector<NtStatus, 0>;
  /// Used to exclude subgroups in dumps.
  using SGExclusionSet = llvm::SmallSet<Subgroup, 4>;
public:
  NtCodeParser(llvm::MemoryBufferRef MBRef) :
   SPBuf(MBRef.getBuffer()), SPBufID(MBRef.getBufferIdentifier()) { }
  
  static bool FindAndConsume(StringRef& Str, StringRef ToFind);
  static StringRef FindAndTake(StringRef& Str, StringRef ToFind);
  static StringRef GetSubgroupPrefix(Subgroup SG);
  static bool IsLargeGroup(const StatusGroupVec& Statuses);
  static size_t GetLargeGroupSize();
  /// For tweaking the level of dispersal.
  static void SetLargeGroupSize(size_t Size);
  static bool InStatusSubgroup(const NtStatus& Status);

  [[nodiscard]] bool parseFile();
  void dumpGroups(std::initializer_list<Subgroup> Exs = {}) const;
  void dumpGroups(const SGExclusionSet& Exclude) const;
  [[nodiscard]] bool writeToFile(StringRef Filename, bool Debug = false);

  [[nodiscard]] bool parseSuccessful() const { 
    return this->didParseSuccessfully;
  }
  [[nodiscard]] StringRef getBufferID() const {
    return this->SPBufID;
  }

private:
  bool mapCodeGroup(StatusGroup G, NtStatus& Code);
  std::optional<StringRef> consumeNextSection();
  std::optional<CodePair> parseSection(StringRef Section);
  bool emitGroupData(llvm::raw_ostream& OS);

  void dumpGroup(StringRef GroupName, 
    const StatusGroupVec& Statuses, 
    const SGExclusionSet& Exclude) const;

private:
  StringRef SPBuf;
  StringRef SPBufID;
  bool didParseSuccessfully = false;

  std::set<uint32_t> parsedValues;
  bool hadDuplicate = false;

  StatusGroupVec successes;
  StatusGroupVec infos;
  StatusGroupVec warnings;
  StatusGroupVec errors;
};
