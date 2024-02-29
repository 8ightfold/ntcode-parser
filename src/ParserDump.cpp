//===- ParserDump.cpp -----------------------------------------------===//
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

#include "Parser.hpp"
#include "llvm/Support/Format.h"
#include "llvm/Support/WithColor.h"

using namespace llvm;

static constinit size_t largeGroupSize = 64;

static bool DoParserDump(const NtCodeParser* Parser);
static void InsertExclusion(NtCodeParser::SGExclusionSet& Ex, Subgroup SG);
static std::pair<StringRef, StringRef> GetSGPrefixRemoved(const NtStatus& Status);
static WithColor GetColorRAII(const NtStatus& Status);

void NtCodeParser::dumpGroups(std::initializer_list<Subgroup> Exs) const {
  if (!DoParserDump(this))
    return;
  SGExclusionSet Exclude {};
  for (Subgroup SG : Exs)
    InsertExclusion(Exclude, SG);
  dumpGroups(Exclude);
}

void NtCodeParser::dumpGroups(const SGExclusionSet& Exclude) const {
  if (!DoParserDump(this))
    return;
  dumpGroup("Success", successes, Exclude);
  dumpGroup("Info",    infos,     Exclude);
  dumpGroup("Warning", warnings,  Exclude);
  dumpGroup("Error",   errors,    Exclude);
}

void NtCodeParser::dumpGroup(
 StringRef GroupName, const StatusGroupVec& Statuses,
 const SGExclusionSet& Exclude) const {
  outs() << "Group<" << GroupName << ">" 
    << (IsLargeGroup(Statuses) ? "" : "*")  << ": {\n";
  for (const NtStatus& Status : Statuses) {
    if (Exclude.contains(Status.SG))
      continue;
    WithColor OS {GetColorRAII(Status)};
    auto [Prefix, Name] = GetSGPrefixRemoved(Status);
    outs() << "  - ["
      << center_justify(Prefix, 8)
      << "] " << Name << ": " 
      << format_hex(Status.Code, 5, true) << "\n";
  }
  outs() << "}\n\n";
}

//=== Statics ===//

bool NtCodeParser::IsLargeGroup(const StatusGroupVec& Statuses) {
  return Statuses.size() > largeGroupSize;
}
size_t NtCodeParser::GetLargeGroupSize() {
  return largeGroupSize;
}
void NtCodeParser::SetLargeGroupSize(size_t Size) {
  largeGroupSize = Size;
}

bool NtCodeParser::InStatusSubgroup(const NtStatus& Status) {
  switch (Status.SG) {
   case Subgroup::DBG:  [[fallthrough]];
   case Subgroup::RPCA: [[fallthrough]];
   case Subgroup::RPCB: return false;
   default:             return true;
  }
}

void InsertExclusion(NtCodeParser::SGExclusionSet& Ex, Subgroup SG) {
  if (SG == Subgroup::RPC) {
    Ex.insert(Subgroup::RPCA);
    Ex.insert(Subgroup::RPCB);
  } else if (SG == Subgroup::NDIS) {
    Ex.insert(Subgroup::NDISA);
    Ex.insert(Subgroup::NDISB);
    Ex.insert(Subgroup::NDISC);
  } else if (SG == Subgroup::IPSEC) {
    Ex.insert(Subgroup::IPSECA);
    Ex.insert(Subgroup::IPSECB);
  } else {
    Ex.insert(SG);
  }
}

bool DoParserDump(const NtCodeParser* Parser) {
  if (!Parser->parseSuccessful()) {
    WithColor::error();
    errs() << "Not dumping \"" << Parser->getBufferID()
      << "\";\neither .parseFile() was never called, "
      << "or an error occurred.\n";
    return false;
  }
  return true;
}

std::pair<StringRef, StringRef>
 GetSGPrefixRemoved(const NtStatus& Status) {
  StringRef Prefix = NtCodeParser::GetSubgroupPrefix(Status.SG);
  StringRef Name   = Status.Name;
  Name.consume_front(Prefix);
  Name.consume_front("_");
  return {Prefix, Name};
}

WithColor GetColorRAII(const NtStatus& Status) {
  const bool NStatus = !NtCodeParser::InStatusSubgroup(Status);
  const bool FormatS = Status.Message.contains('%');
  raw_ostream::Colors Color = raw_ostream::WHITE;

  if (NStatus && FormatS)
    Color = raw_ostream::GREEN;
  else if (NStatus)
    Color = raw_ostream::CYAN;
  else if (FormatS)
    Color = raw_ostream::YELLOW;
  
  return WithColor(outs(), Color);
}