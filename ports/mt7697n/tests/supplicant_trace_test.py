"""Reject stale offsets/signatures in temporary SDK boundary tracing."""
from pathlib import Path
import re
import sys
from elftools.elf.elffile import ELFFile
from elftools.dwarf.dwarf_expr import DWARFExprParser

port=Path(__file__).resolve().parents[1]
image = Path(sys.argv[1]) if len(sys.argv) > 1 else port/'build/tasmota-gcc13-sdk-runtime/tasmota.elf'
with image.open('rb') as stream:
    elf=ELFFile(stream)
    dwarf=elf.get_dwarf_info()
    layouts=set()
    signatures={}
    names={'wpa_supplicant_add_iface':2,'wpa_config_read':1,
           'wpa_config_alloc_new_conf':1,'os_strlcpy':3,'os_zalloc':1}
    for cu in dwarf.iter_CUs():
        for die in cu.iter_DIEs():
            attribute=die.attributes.get('DW_AT_name')
            if not attribute:continue
            name=attribute.value.decode()
            if die.tag=='DW_TAG_subprogram' and name in names and 'DW_AT_low_pc' in die.attributes:
                signatures[name]=sum(c.tag=='DW_TAG_formal_parameter' for c in die.iter_children())
            if die.tag!='DW_TAG_structure_type' or name!='wpa_supplicant':continue
            size=die.attributes.get('DW_AT_byte_size')
            if not size:continue
            for member in die.iter_children():
                name=member.attributes.get('DW_AT_name')
                if not name or name.value!=b'eapol':continue
                location=member.attributes['DW_AT_data_member_location'].value
                if not isinstance(location,int):
                    ops=DWARFExprParser(cu.structs).parse_expr(location)
                    assert len(ops)==1 and ops[0].op_name=='DW_OP_plus_uconst'
                    location=ops[0].args[0]
                layouts.add((size.value,location))
    assert layouts=={(616,296)},layouts
    assert signatures==names,signatures
    symbols={s.name:s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
    for name in names:
        assert symbols['__wrap_'+name] and symbols[name]!=symbols['__wrap_'+name]
source=(port/'platform/supplicant_trace.cpp').read_text()
assert re.search(r'station_size\s*=\s*616;',source)
assert re.search(r'eapol_offset\s*=\s*296;',source)
print('Supplicant tracing: linked SDK layout, parameter counts and wrappers verified')
