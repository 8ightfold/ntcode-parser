//===- Emitter.cpp --------------------------------------------------===//
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

#include "Emitter.hpp"
#include "llvm/Support/Format.h"
#include "llvm/Support/WithColor.h"

using namespace llvm;

using StatusGroupRef = GroupEmitter::StatusGroupRef;
using EmitterMsgType = GroupEmitter::EmitterMsgType;

static StringRef GetGroupName(StatusGroup Group);
static std::string MakePascalcase(StringRef Name);
static llvm::FormattedNumber FormatMergedCode(const NtStatus& Status);
static uint32_t MergeStatusAndCode(const NtStatus& Status);

namespace {
  template <typename T>
  struct BindColor {
    BindColor(T Val, raw_ostream::Colors Color) :
     storedValue(std::forward<T>(Val)), color(Color) { }
  public:
    raw_ostream::Colors getColor() const { return color; }
    T getValue() { return std::forward<T>(storedValue); }
  private:
    T storedValue;
    raw_ostream::Colors color;
  };

  template <typename T>
  BindColor(T&&) -> BindColor<T>;

  template <typename T>
  WithColor& operator<<(WithColor& OS, BindColor<T> B) {
    return OS.changeColor(B.getColor())
      << B.getValue() << raw_ostream::GREEN;
  }
} // namespace `anonymous`


GroupEmitter::GroupEmitter(raw_ostream& OS) : OS(OS) {
  this->isDebug = OS.is_displayed();
  if (isDebug)
    outs() << "Debugging." << '\n';
}

WithColor GroupEmitter::idbgs() const {
  if (!this->isDebug)
    return WithColor(nulls());
  return WithColor(OS, raw_ostream::GREEN);
}

//=== Implementation ===//

void GroupEmitter::emit(
 StatusGroup G, StatusGroupRef Statuses) {
  groupName = GetGroupName(G);
  if (!doEmit(G, Statuses)) {
    didEmitSuccessfully = false;
    failures.push_back(groupName);
  }
}

bool GroupEmitter::doEmit(
 StatusGroup G, StatusGroupRef Statuses) {
  if (NtCodeParser::IsLargeGroup(Statuses))
    return groupedEmit(G, Statuses);
  return linearEmit(G, Statuses);
}

const EmitterMsgType& GroupEmitter::formatMessage(const NtStatus& Status) {
  storedMsg.clear();
  size_t Pos = 0;
  StringRef Msg = Status.Message;
  SmallString<64> MsgFragment;
  
  while ((Pos = Msg.find("\r\n")) != StringRef::npos) {
    storedMsg.append(Msg.take_front(Pos));
    Msg = Msg.drop_front(Pos + 2);
  }

  storedMsg.append(Msg);
  return storedMsg;
}

// emitters

void GroupEmitter::emitTableSwitchPair(
 StatusSpan Statuses, StringRef FuncName, StringRef TableName) {
  emitTable(Statuses, "table");
  OS.indent(2) << "static OpaqueError "
    << FuncName << "(OpqErrorID ID) {\n";
  emitSwitch(Statuses, "table");
  OS.indent(2) << "}\n";
}

void GroupEmitter::emitTable(
 StatusSpan Statuses, StringRef Name) {
  OS.indent(2) << "static constexpr IOpaqueError "
    << Name << "[] {\n";
  for (const NtStatus& Status : Statuses.drop_back())
    emitTableValue(Status);
  emitTableValue(Statuses.back(), true);
  OS.indent(2) << "};\n\n";
}

void GroupEmitter::emitTableValue(const NtStatus& Status, bool NoComma) {
  OS.indent(4) << "$NewPErr(\"" << MakePascalcase(Status.Name) << '\"'
    << ", \"" << formatMessage(Status) << "\")"
    << (NoComma ? "" : ",") << '\n';
}

void GroupEmitter::emitSwitch(StatusSpan Statuses, StringRef TableName) {
  OS.indent(4) << "switch (ID) {\n";
  for (uint64_t Ix = 0; Ix < Statuses.size(); ++Ix)
    emitSwitchValue(Statuses[Ix], TableName, Ix);
  OS.indent(5) << "default: return nullptr;\n";
  OS.indent(4) << "}\n";
}

void GroupEmitter::emitSwitchValue(
 const NtStatus& Status, StringRef TableName, uint64_t Ix) {
  OS.indent(5) << "case " << FormatMergedCode(Status)
    << ": return &" << TableName << "[" << Ix << "];\n";
}

// linear

bool GroupEmitter::linearEmit(
 StatusGroup G, StatusGroupRef Statuses) {
  idbgs() << "Group "
    << BindColor(groupName, YELLOW)
    << " is linear (Size: " << Statuses.size() << ").\n";

  OS << "#define CURR_SEVERITY " << groupName << "\n";
  OS << "struct _" << groupName << "Group {\n";
  emitTableSwitchPair(Statuses, "Get", "table");
  OS << "};\n" << "#undef CURR_SEVERITY\n\n";

  return true;
}

// grouped

bool GroupEmitter::groupedEmit(
 StatusGroup G, StatusGroupRef Statuses) {
  idbgs() << "Group " 
    << BindColor(GetGroupName(G), YELLOW)
    << " is batched (Size: " << Statuses.size() << ").\n";
  
  bool WasSuccessful = true;

  WasSuccessful = false;
  return WasSuccessful;
}

//=== Statics ===//

StringRef GetGroupName(StatusGroup Group) {
  switch (Group) {
   case StatusGroup::SUCCESS:
    return "Success";
   case StatusGroup::INFO:
    return "Info";
   case StatusGroup::WARNING:
    return "Warning";
   case StatusGroup::ERROR:
    return "Error";
   default:
    return "Unknown";
  }
}

std::string MakePascalcase(StringRef Name) {
  std::string Output;
  size_t Pos = 0;
  Output.reserve(Name.size());

  while ((Pos = Name.find('_')) != StringRef::npos) {
    StringRef Substr = Name.take_front(Pos);
    Output.push_back(Substr[0]);
    for (char C : Substr.drop_front())
      Output.push_back(std::tolower(C));
    Name = Name.drop_front(Pos + 1);
  }

  if (!Name.empty()) {
    Output.push_back(Name[0]);
    for (char C : Name.drop_front())
      Output.push_back(std::tolower(C));
  }
  return Output;
}

llvm::FormattedNumber FormatMergedCode(const NtStatus& Status) {
  const uint32_t SC = MergeStatusAndCode(Status);
  return llvm::format_hex(SC, 8, true);
}

uint32_t MergeStatusAndCode(const NtStatus& Status) {
  auto RawSG = uint32_t(Status.SG) << (3 * 4);
  return (RawSG | Status.Code);
}
