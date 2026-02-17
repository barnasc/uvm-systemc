//----------------------------------------------------------------------
//   Copyright 2026 COSEDA Technologies GmbH
//   All Rights Reserved Worldwide
//
//   Licensed under the Apache License, Version 2.0 (the
//   "License"); you may not use this file except in
//   compliance with the License.  You may obtain a copy of
//   the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in
//   writing, software distributed under the License is
//   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//   CONDITIONS OF ANY KIND, either express or implied.  See
//   the License for the specific language governing
//   permissions and limitations under the License.
//----------------------------------------------------------------------

#include <systemc>
#include <uvm>

#include <vector>
#include <sstream>
#include <string>

using namespace uvm;

class reg_Ra : public uvm_reg
{
public:
  uvm_reg_field* F1{nullptr};
  uvm_reg_field* F2{nullptr};

  reg_Ra(const std::string& name = "Ra")
  : uvm_reg(name, 32, UVM_NO_COVERAGE)
  {}

  void build() 
  {
    F1 = uvm_reg_field::type_id::create("F1");
    F1->configure(this, 8, 0, "RW", false, 0x0, true, false, true);
    F2 = uvm_reg_field::type_id::create("F2");
    F2->configure(this, 8, 16, "RO", false, 0x0, true, false, true);
  }

  UVM_OBJECT_UTILS(reg_Ra);
};

class reg_Rb : public uvm_reg
{
public:
  uvm_reg_field* F1{nullptr};
  uvm_reg_field* F2{nullptr};

  reg_Rb(const std::string& name = "Rb")
  : uvm_reg(name, 32, UVM_NO_COVERAGE)
  {}

  void build() 
  {
    F1 = uvm_reg_field::type_id::create("F1");
    F1->configure(this, 8, 0, "RW", false, 0x0, true, false, true);
    F2 = uvm_reg_field::type_id::create("F2");
    F2->configure(this, 8, 16, "RW", false, 0x0, true, false, true);
  }

  UVM_OBJECT_UTILS(reg_Rb);
};

class block_B : public uvm_reg_block
{
public:
  reg_Ra* Ra{nullptr};
  reg_Rb* Rb{nullptr};

  block_B(const std::string& name = "B")
  : uvm_reg_block(name, UVM_NO_COVERAGE)
  {}

  void build() 
  {
    default_map = create_map("default_map", 0, 4, UVM_BIG_ENDIAN);

    Ra = reg_Ra::type_id::create("Ra");
    Ra->configure(this, nullptr);
    Ra->build();

    Rb = reg_Rb::type_id::create("Rb");
    Rb->configure(this, nullptr);
    Rb->build();

    default_map->add_reg(Ra, 0x0, "RW");
    default_map->add_reg(Rb, 0x100, "RW");

    lock_model();
  }

  UVM_OBJECT_UTILS(block_B);
};

class top_block : public uvm_reg_block
{
public:
  block_B* child{nullptr};

  top_block(const std::string& name = "top_block")
  : uvm_reg_block(name, UVM_NO_COVERAGE)
  {}

  ~top_block()
  {
    if (child) {
      delete child;
    }
  }

  UVM_OBJECT_UTILS(top_block);

  void build() 
  {
    default_map = create_map("top_map", 0, 4, UVM_LITTLE_ENDIAN);

    child = block_B::type_id::create("child", nullptr, get_full_name());
    child->build();
    child->configure(this, "dut");

    default_map->add_submap(child->default_map, 0x200);
    lock_model();
  }
};

class bug_343_get_physical_addresses_test : public uvm_test
{
public:
  top_block* model{nullptr};

  bug_343_get_physical_addresses_test( uvm::uvm_component_name name = "bug_343_get_physical_addresses_test")
  : uvm_test(name)
  {}

  UVM_COMPONENT_UTILS(bug_343_get_physical_addresses_test);

  void build_phase(uvm_phase& phase) override
  {
    uvm_test::build_phase(phase);
    model = top_block::type_id::create("model");
    model->build();
  }

  void run_phase(uvm_phase& phase) override
  {
    phase.raise_objection(this);

    uvm_reg_map* map = model->child->default_map;
    std::vector<uvm_reg_addr_t> addrs;

    log_translation(map, addrs, 0x0, {uvm_reg_addr_t(0x200)}, "reg_ra", 4);
    log_translation(map, addrs, 0x100, {uvm_reg_addr_t(0x300)}, "reg_rb", 4);
    log_translation(map, addrs, 0x0, {uvm_reg_addr_t(0x204), uvm_reg_addr_t(0x200)}, "burst_first", 8);
    log_translation(map, addrs, 0x100, {uvm_reg_addr_t(0x304), uvm_reg_addr_t(0x300)}, "burst_second", 8);

    phase.drop_objection(this);
  }

private:
  std::string format_addrs(const std::vector<uvm_reg_addr_t>& addrs) const
  {
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < addrs.size(); ++i) {
      if (i != 0) {
        out << ", ";
      }
      out << "0x" << addrs[i].to_string(sc_dt::SC_HEX);
    }
    out << "]";
    return out.str();
  }

  void log_translation(uvm_reg_map* map,
                       std::vector<uvm_reg_addr_t>& addrs,
                       uvm_reg_addr_t local_base,
                       const std::vector<uvm_reg_addr_t>& expected_addrs,
                       const std::string& tag,
                       unsigned int n_bytes)
  {
    map->get_physical_addresses(local_base, 0, n_bytes, addrs);

    std::ostringstream header;
    header << "get_physical_addresses " << tag
           << " local_base=0x" << local_base.to_string(sc_dt::SC_HEX)
           << " n_bytes=" << std::dec << n_bytes;
    UVM_INFO("BUG343", header.str(), UVM_LOW);

    std::ostringstream expected;
    expected << "Expected addr list: " << format_addrs(expected_addrs);
    UVM_INFO("BUG343", expected.str(), UVM_LOW);

    std::ostringstream actual;
    actual << "Actual addr list:   " << format_addrs(addrs)
           << " (size=" << std::dec << addrs.size() << ")";
    UVM_INFO("BUG343", actual.str(), UVM_LOW);
  }
};

int sc_main(int, char*[])
{
  uvm::run_test("bug_343_get_physical_addresses_test");
  return 0;
}
