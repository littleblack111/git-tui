#include <string>  // for operator==, basic_string, allocator, string

#include "diff.hpp"     // for main
#include "git.hpp"      // for main
#include "help.hpp"     // for main
#include "log.hpp"      // for main
#include "version.hpp"  // for main

int main(int argc, const char** argv) {
  if (argc == 0)
    return gittui::help::main(argc, argv);

  // Eat the first argument.
  argc--;
  argv++;

  if (argc == 0)
    return gittui::help::main(argc, argv);

  std::string command = argv[0];

  // Eat the second argument.
  argc--;
  argv++;

  if (command == "diff")
    return gittui::diff::main(argc, argv);

  if (command == "log")
    return gittui::log::main(argc, argv);

  if (command == "version" || command == "--version" || command == "-v")
    return gittui::version::main(argc, argv);

  if (command == "help" || command == "--help" || command == "-h")
    return gittui::help::main(argc, argv);

  if (command == "git") {
    return gittui::git::main(argc, argv);
  }

  // Unknown command, fallback to git, uneat the first argument.
  argc++;
  argv--;
  return gittui::git::main(argc, argv);
}

// Copyright 2021 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
