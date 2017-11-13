/* Copyright 2017 R. Thomas
 * Copyright 2017 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <algorithm>
#include <list>
#include <fstream>
#include <iterator>

#include "LIEF/logging++.hpp"

#include "LIEF/MachO/Builder.hpp"
#include "LIEF/exception.hpp"

#include "Builder.tcc"
#include "Object.tcc"


namespace LIEF {
namespace MachO {

Builder::~Builder(void) = default;

Builder::Builder(Binary *binary) {
  this->raw_.reserve(binary->original_size());
  this->binaries_.push_back(std::move(binary));
  this->binary_ = binary;
  if (this->binary_->is64_) {
    this->build<MachO64>();
  } else {
    this->build<MachO32>();
  }
}

Builder::Builder(std::vector<Binary*> binaries) {
  this->binaries_ = binaries;
  this->binary_   = this->binaries_.back();
  if (this->binary_->is64_) {
    this->build<MachO64>();
  } else {
    this->build<MachO32>();
  }
}


template <typename T>
void Builder::build(void) {
  if (this->binaries_.size() > 1) {
    throw not_supported("Actually, builder only support single binary");
  }


  this->build_uuid();

  for (LoadCommand* cmd : this->binary_->commands_) {
    if (cmd->is<DylibCommand>()) {
      this->build<T>(cmd->as<DylibCommand>());
      continue;
    }

    if (cmd->is<DylinkerCommand>()) {
      this->build<T>(cmd->as<DylinkerCommand>());
      continue;
    }

    if (cmd->is<VersionMin>()) {
      this->build<T>(cmd->as<VersionMin>());
      continue;
    }

    if (cmd->is<SourceVersion>()) {
      this->build<T>(cmd->as<SourceVersion>());
      continue;
    }

    if (cmd->is<MainCommand>()) {
      this->build<T>(cmd->as<MainCommand>());
      continue;
    }

    if (cmd->is<DyldInfo>()) {
      this->build<T>(cmd->as<DyldInfo>());
      continue;
    }

    if (cmd->is<FunctionStarts>()) {
      this->build<T>(cmd->as<FunctionStarts>());
      continue;
    }

    if (cmd->is<SymbolCommand>()) {
      this->build<T>(cmd->as<SymbolCommand>());
      continue;
    }

    if (cmd->is<DynamicSymbolCommand>()) {
      this->build<T>(cmd->as<DynamicSymbolCommand>());
      continue;
    }

    if (cmd->is<DataInCode>()) {
      this->build<T>(cmd->as<DataInCode>());
      continue;
    }

    if (cmd->is<CodeSignature>()) {
      this->build<T>(cmd->as<CodeSignature>());
      continue;
    }

    if (cmd->is<SegmentSplitInfo>()) {
      this->build<T>(cmd->as<SegmentSplitInfo>());
      continue;
    }

    if (cmd->is<SubFramework>()) {
      this->build<T>(cmd->as<SubFramework>());
      continue;
    }

    if (cmd->is<DyldEnvironment>()) {
      this->build<T>(cmd->as<DyldEnvironment>());
      continue;
    }
  }

  this->build_segments<T>();
  this->build_load_commands();
  //this->build_symbols<T>();

  this->build_header();
}


void Builder::build_header(void) {
  VLOG(VDEBUG) << "[+] Building header" << std::endl;
  const Header& binary_header = this->binary_->header();
  if (this->binary_->is64_) {
    mach_header_64 header;
    header.magic      = static_cast<uint32_t>(binary_header.magic());
    header.cputype    = static_cast<uint32_t>(binary_header.cpu_type());
    header.cpusubtype = static_cast<uint32_t>(binary_header.cpu_subtype());
    header.filetype   = static_cast<uint32_t>(binary_header.file_type());
    header.ncmds      = static_cast<uint32_t>(binary_header.nb_cmds());
    header.sizeofcmds = static_cast<uint32_t>(binary_header.sizeof_cmds());
    header.flags      = static_cast<uint32_t>(binary_header.flags());
    header.reserved   = static_cast<uint32_t>(binary_header.reserved());

    this->raw_.seekp(0);
    this->raw_.write(reinterpret_cast<const uint8_t*>(&header), sizeof(mach_header_64));
  } else {
    mach_header header;
    header.magic      = static_cast<uint32_t>(binary_header.magic());
    header.cputype    = static_cast<uint32_t>(binary_header.cpu_type());
    header.cpusubtype = static_cast<uint32_t>(binary_header.cpu_subtype());
    header.filetype   = static_cast<uint32_t>(binary_header.file_type());
    header.ncmds      = static_cast<uint32_t>(binary_header.nb_cmds());
    header.sizeofcmds = static_cast<uint32_t>(binary_header.sizeof_cmds());
    header.flags      = static_cast<uint32_t>(binary_header.flags());

    this->raw_.seekp(0);
    this->raw_.write(reinterpret_cast<const uint8_t*>(&header), sizeof(mach_header));
  }

}


void Builder::build_load_commands(void) {
  VLOG(VDEBUG) << "[+] Building load segments" << std::endl;

  const auto& binary = this->binaries_.back();
  // Check if the number of segments is correct
  if (binary->header().nb_cmds() != binary->commands_.size()) {
    LOG(WARNING) << "Error: header.nb_cmds = " << std::dec << binary->header().nb_cmds()
                 << " and number of commands is " << binary->commands_.size() << std::endl;
    throw LIEF::builder_error("");
  }


  for (const SegmentCommand& segment : binary->segments()) {
    const std::vector<uint8_t>& segment_content = segment.content();
    this->raw_.seekp(segment.file_offset());
    this->raw_.write(segment_content);
  }

  //uint64_t loadCommandsOffset = this->raw_.size();
  for (const LoadCommand& command : binary->commands()) {
    auto& data = command.data();
    uint64_t loadCommandsOffset = command.command_offset();
    VLOG(VDEBUG) << "[+] Command offset: 0x" << std::hex << loadCommandsOffset << std::endl;
    this->raw_.seekp(loadCommandsOffset);
    this->raw_.write(data);
  }
}

void Builder::build_uuid(void) {
  auto&& uuid_it = std::find_if(
        std::begin(this->binary_->commands_),
        std::end(this->binary_->commands_),
        [] (const LoadCommand* command) {
          return (typeid(*command) == typeid(UUIDCommand));
        });
  if (uuid_it == std::end(this->binary_->commands_)) {
    VLOG(VDEBUG) << "[-] No uuid" << std::endl;
    return;
  }

  UUIDCommand* uuid_cmd = dynamic_cast<UUIDCommand*>(*uuid_it);
  uuid_command raw_cmd;
  std::fill(
      reinterpret_cast<uint8_t*>(&raw_cmd),
      reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(uuid_command),
      0);

  raw_cmd.cmd     = static_cast<uint32_t>(uuid_cmd->command());
  raw_cmd.cmdsize = static_cast<uint32_t>(uuid_cmd->size()); // sizeof(uuid_command)

  const uuid_t& uuid = uuid_cmd->uuid();
  std::copy(std::begin(uuid), std::end(uuid), raw_cmd.uuid);

  if (uuid_cmd->size() < sizeof(uuid_command)) {
    LOG(WARNING) << "Size of original data is different for " << to_string(uuid_cmd->command()) << ": Skip!";
    return;
  }

  std::copy(
      reinterpret_cast<uint8_t*>(&raw_cmd),
      reinterpret_cast<uint8_t*>(&raw_cmd) + sizeof(uuid_command),
      uuid_cmd->originalData_.data());
}


const std::vector<uint8_t>& Builder::get_build(void) {
  return this->raw_.raw();
}


void Builder::write(MachO::Binary *binary, const std::string& filename) {
  Builder builder{binary};
  builder.write(filename);
}

void Builder::write(const std::string& filename) const {

  std::ofstream output_file{filename, std::ios::out | std::ios::binary | std::ios::trunc};
  if (output_file) {
    std::vector<uint8_t> content;
    this->raw_.get(content);

    std::copy(
        std::begin(content),
        std::end(content),
        std::ostreambuf_iterator<char>(output_file));
  }

}

}
}
