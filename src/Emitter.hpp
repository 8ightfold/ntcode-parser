//===- Emitter.hpp --------------------------------------------------===//
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

#include "Parser.hpp"
#include "llvm/ADT/SmallString.h"

using StatusSpan = llvm::ArrayRef<NtStatus>;
namespace llvm { struct WithColor; }

struct GroupEmitter {
  using enum StatusGroup;
  using enum llvm::raw_ostream::Colors;
  using StatusGroupRef = const NtCodeParser::StatusGroupVec&;
  using EmitterMsgType = llvm::SmallString<128>;
public:
  GroupEmitter(llvm::raw_ostream& OS);
public:
  void emit(StatusGroup G, StatusGroupRef Statuses);
  bool doEmit(StatusGroup G, StatusGroupRef Statuses);
  const EmitterMsgType& formatMessage(const NtStatus& Status);

  void emitTableSwitchPair(StatusSpan Statuses, StringRef FuncName, StringRef TableName);
  void emitTable(StatusSpan Statuses, StringRef Name);
  void emitTableValue(const NtStatus& Status, bool NoComma = false);
  void emitSwitch(StatusSpan Statuses, StringRef TableName);
  void emitSwitchValue(const NtStatus& Status, StringRef TableName, uint64_t Ix);


  bool linearEmit(StatusGroup G, StatusGroupRef Statuses);
  bool groupedEmit(StatusGroup G, StatusGroupRef Statuses);

  [[nodiscard]] bool emitSuccessful() const { 
    return this->didEmitSuccessfully;
  }
  [[nodiscard]] llvm::ArrayRef<StringRef> getFailures() const { 
    return this->failures;
  }

private:
  llvm::WithColor idbgs() const;

private:
  llvm::raw_ostream& OS;
  bool isDebug = false;
  bool didEmitSuccessfully = true;
  llvm::SmallVector<StringRef, 4> failures;

  StringRef groupName;
  EmitterMsgType storedMsg;
};
