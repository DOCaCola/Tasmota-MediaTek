from elftools.elf.elffile import ELFFile
from elftools.dwarf.dwarf_expr import DWARFExprParser
from pathlib import Path
import sys
p=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[1]/'build/tasmota-gcc13-sdk-runtime/tasmota.elf'
with p.open('rb') as f:
 d=ELFFile(f).get_dwarf_info();layouts={}
 for cu in d.iter_CUs():
  name=cu.get_top_DIE().attributes.get('DW_AT_name').value.decode(errors='replace')
  for die in cu.iter_DIEs():
   if die.tag!='DW_TAG_structure_type' or die.attributes.get('DW_AT_name',None) is None or die.attributes['DW_AT_name'].value not in (b'tcp_pcb',b'stats_',b'stats_mem') or 'DW_AT_byte_size' not in die.attributes:continue
   fields={}
   for m in die.iter_children():
    if 'DW_AT_data_member_location' not in m.attributes:continue
    loc=m.attributes['DW_AT_data_member_location'].value
    if not isinstance(loc,int):loc=DWARFExprParser(cu.structs).parse_expr(loc)[0].args[0]
    fields[m.attributes['DW_AT_name'].value.decode()]=loc
   kind=die.attributes['DW_AT_name'].value.decode()
   key=(kind,die.attributes['DW_AT_byte_size'].value,tuple(sorted(fields.items())))
   layouts.setdefault(key,[]).append(name)
 for (kind,size,fields),names in layouts.items():
  fields=dict(fields)
  if kind=='stats_':assert size==196 and fields['mem']==168
  if kind=='stats_mem':assert size==10 and fields['err']==0 and fields['used']==4 and fields['max']==6
 assert sum(k[0]=='tcp_pcb' for k in layouts)==1, 'TCP PCB ABI mismatch'
 assert {k[0] for k in layouts}=={'tcp_pcb','stats_','stats_mem'}
 print('Linked TCP PCB and SDK pool-statistics layouts verified')
