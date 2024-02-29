//===- ParserHead.cpp -----------------------------------------------===//
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

static StatusGroup ConsumeStatusGroup(uint32_t& GroupAndCode);
static StatusCtx   ConsumeStatusCtx(uint32_t& GroupAndCode);

bool NtCodeParser::parseFile() {
  bool ParseSuccess = true;
  while (auto OSection = consumeNextSection()) {
    auto OCode = parseSection(*OSection);
    if (!OCode) {
      if (!this->hadDuplicate)
        ParseSuccess = false;
      hadDuplicate = false;
      continue;
    }
    auto& [G, Code] = *OCode;
    if (!mapCodeGroup(G, Code))
      ParseSuccess = false;
  }

  this->didParseSuccessfully = ParseSuccess;
  return ParseSuccess;
}

bool NtCodeParser::mapCodeGroup(StatusGroup Group, NtStatus& Code) {
  switch (Group) {
   case StatusGroup::SUCCESS: {
    successes.emplace_back(Code);
    break;
   }
   case StatusGroup::INFO: {
    infos.emplace_back(Code);
    break;
   }
   case StatusGroup::WARNING: {
    warnings.emplace_back(Code);
    break;
   }
   case StatusGroup::ERROR: {
    errors.emplace_back(Code);
    break;
   }
   default: {
    WithColor::error();
    const auto GroupID = static_cast<uint8_t>(Group);
    errs() << "Invalid CodeGroup: "
      << llvm::format_hex(GroupID, 4, true) << ".\n";
    return false;
   }
  }

  return true;
}

std::optional<StringRef> NtCodeParser::consumeNextSection() {
  constexpr size_t npos = StringRef::npos;
  const size_t Beg = SPBuf.find("<tr>");
  if (Beg == npos)
    return std::nullopt;
  SPBuf = SPBuf.drop_front(Beg);
  if (auto S = FindAndTake(SPBuf, "</tr>"); !S.empty())
    return {S};
  return std::nullopt;
}

std::optional<NtCodeParser::CodePair>
 NtCodeParser::parseSection(StringRef Section) {
  auto Err = [&Section] (StringRef Prefix, StringRef End = "</p>") {
    WithColor::error();
    const size_t EndPos = Section.find(End);
    if (EndPos == StringRef::npos) {
      errs() << Prefix << ".\n";
      return;
    }
    errs() << Prefix << ": " 
      << Section.take_front(EndPos) << ".\n";
  };

  NtStatus Status;
  StatusGroup Group;

  if (!FindAndConsume(Section, "<p>")) {
    WithColor::error();
    errs() << "Couldn't locate status code.\n";
    return std::nullopt;
  }
  Section.consume_front("0x");

  uint32_t GroupAndCode;
  if (Section.consumeInteger(16, GroupAndCode)) {
    Err("Invalid status or group");
    return std::nullopt;
  }

  if (parsedValues.contains(GroupAndCode)) {
    this->hadDuplicate = true;
    return std::nullopt;
  }
  parsedValues.insert(GroupAndCode);

  auto Ctx = ConsumeStatusCtx(GroupAndCode);
  Group       = Ctx.Group;
  Status.Code = GroupAndCode;
  Status.SG   = Ctx.SG;

  if (GetSubgroupPrefix(Ctx.SG).empty()) {
    WithColor::error();
    errs() << "Invalid subgroup.\n";
    return std::nullopt;
  }

  if (!FindAndConsume(Section, "<p>")) {
    WithColor::error();
    errs() << "Couldn't locate status name.\n";
    return std::nullopt;
  }

  const size_t NameEnd = Section.find("</p>");
  if (NameEnd == StringRef::npos) {
    WithColor::error();
    errs() << "Couldn't locate status name end.\n";
    return std::nullopt;
  }
  Status.Name = Section.take_front(NameEnd);
  Status.Name.consume_front("STATUS_");
  Section.consume_front("</p>");
  
  if (!FindAndConsume(Section, "<p>")) {
    WithColor::error();
    errs() << "Couldn't locate status message.\n";
    return std::nullopt;
  }
  
  const size_t MsgEnd = Section.find("</p>");
  if (MsgEnd == StringRef::npos) {
    WithColor::error();
    errs() << "Couldn't locate status message end.\n";
    return std::nullopt;
  }
  Status.Message = Section.take_front(MsgEnd);
  return {{Group, Status}};
}

//=== Statics ===//

bool NtCodeParser::FindAndConsume(StringRef& Str, StringRef ToFind) {
  const size_t Off = Str.find(ToFind);
  if (Off == StringRef::npos)
    return false;
  Str = Str.drop_front(Off + ToFind.size());
  return true;
}

StringRef NtCodeParser::FindAndTake(StringRef& Str, StringRef ToFind) {
  const size_t Off = Str.find(ToFind);
  if (Off == StringRef::npos)
    return "";
  StringRef ToTake = Str.take_front(Off);
  Str = Str.drop_front(Off + ToFind.size());
  return ToTake;
}

StringRef NtCodeParser::GetSubgroupPrefix(Subgroup SG) {
  switch (SG) {
   case Subgroup::STATUS:   return "STATUS";
   case Subgroup::WOW:      return "WOW";
   case Subgroup::INVALID:  return "INVALID";
   case Subgroup::DBG:      return "DBG";
   case Subgroup::RPCA:
   case Subgroup::RPCB:     return "RPC";
   case Subgroup::PNP:      return "PNP";
   case Subgroup::CTX:      return "CTX";
   case Subgroup::MUI:      return "MUI";
   case Subgroup::CLUSTER:  return "CLUSTER";
   case Subgroup::ACPI:     return "ACPI";
   case Subgroup::FLT:      return "FLT";
   case Subgroup::SXS:      return "SXS";
   case Subgroup::RECOVERY: return "RECOVERY";
   case Subgroup::LOG:      return "LOG";
   case Subgroup::VIDEO:    return "VIDEO";
   case Subgroup::MONITOR:  return "MONITOR";
   case Subgroup::GRAPHICS: return "GRAPHICS";
   case Subgroup::FVE:      return "FVE";
   case Subgroup::FWP:      return "FWP";
   case Subgroup::NDISA:
   case Subgroup::NDISB:
   case Subgroup::NDISC:    return "NDIS";
   case Subgroup::IPSECA:
   case Subgroup::IPSECB:   return "IPSEC";
   case Subgroup::VOLMGR:   return "VOLMGR";
   case Subgroup::VIRTDISK: return "VIRTDISK";
   default:                 return "";
  }
}

StatusGroup ConsumeStatusGroup(uint32_t& GroupAndCode) {
  uint32_t RawStatus = GroupAndCode & 0xF0000000L;
  GroupAndCode &= 0x0FFFFFFFL;
  // Shift status from eighth nibble to first.
  return static_cast<StatusGroup>(RawStatus >> (7 * 4));
}

StatusCtx ConsumeStatusCtx(uint32_t& GroupAndCode) {
  StatusGroup G  = ConsumeStatusGroup(GroupAndCode);
  uint32_t RawSG = GroupAndCode & 0x0FFFF000L;
  GroupAndCode  &= 0x00000FFFL;
  auto SG = static_cast<Subgroup>(RawSG >> (3 * 4));
  return {G, SG};
}
