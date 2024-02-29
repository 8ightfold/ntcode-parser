//===- Driver.cpp ---------------------------------------------------===//
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

#include <Parser.hpp>
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/WithColor.h"

using namespace llvm;

[[noreturn]] static void exitWithError(
 Twine Msg, std::string Hint = "") {
  errs() << raw_ostream::RED << Msg 
    << "\n" << raw_ostream::RESET;
  if (!Hint.empty())
    WithColor::note() << Hint << "\n";
  std::exit(1);
}

[[noreturn]] static void
 exitWithError(Error E) {
  exitWithError(toString(std::move(E)), "");
}

[[noreturn]] static void
 exitWithErrorCode(std::error_code EC) {
  exitWithError(EC.message(), "");
}

static std::pair<bool, SmallString<128>>
 resolveFilePath(StringRef Filename) {
  using namespace llvm::sys;
  SmallString<128> PathConstruct = Filename;
  if (auto EC = fs::make_absolute(PathConstruct))
    exitWithErrorCode(EC);
  if (fs::exists(PathConstruct)) {
    return {true, std::move(PathConstruct)};
  }
  return {false, SmallString<128>{}};
}

int main(int N, char *Argv[]) {
  if (N < 3) {
    exitWithError(
      "Not enough arguments!", 
      "Required: 2");
  }

  bool Found = false;
  SmallString<128> InputPath;
  StringRef OutputName = Argv[2];
  std::tie(Found, InputPath) = resolveFilePath(Argv[1]);

  if (!Found) {
    StringRef Filename(InputPath);
    exitWithError("Could not locate the file \"" + Filename + "\".");
  }

  auto EMBuffer = MemoryBuffer::getFile(InputPath, true);
  if (auto EC = EMBuffer.getError()) {
    StringRef InputPathRef = InputPath;
    exitWithError("Could not open " + InputPathRef 
      + ": " + EC.message());
  }
  
  std::unique_ptr<MemoryBuffer> &MB = *EMBuffer;
  NtCodeParser Parser(MB->getMemBufferRef());
  Parser.SetLargeGroupSize(-1);
  if (!Parser.parseFile())
    exitWithError("Parsing failed.");
  Parser.dumpGroups();
  if (!Parser.writeToFile(OutputName))
    exitWithError("Writing failed.");
}
