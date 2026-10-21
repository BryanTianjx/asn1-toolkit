#!/usr/bin/env python3
"""Independent C PER differential check; requires developer-only asn1tools.

Usage: PYTHONPATH=/path/to/Python_asn1tools python3 scripts/validate_c_per_oracle.py
  --build-dir build-rev1-quick/per-oracle [--write-vectors]
No oracle package is imported by the compiler or runtime. Frozen short vectors
are checked by the dependency-free C11 CTest driver on every normal build.
"""
import argparse
import ctypes as C
import pathlib
import subprocess
import asn1tools

ROOT = pathlib.Path(__file__).resolve().parents[1]
class View(C.Structure):
    _fields_ = [('data', C.POINTER(C.c_uint8)), ('size', C.c_size_t)]
class Big(C.Structure):
    _fields_ = [('sign', C.c_int), ('limbs', C.POINTER(C.c_uint32)), ('nlimbs', C.c_size_t), ('cap', C.c_size_t)]
class Bits(C.Structure):
    _fields_ = [('bits', C.POINTER(C.c_uint8)), ('bit_length', C.c_size_t), ('cap_bytes', C.c_size_t)]
class IntRange(C.Structure):
    _fields_ = [('lower', C.c_int64), ('lower_set', C.c_int), ('upper', C.c_int64), ('upper_set', C.c_int), ('extensible', C.c_int)]
class Size(C.Structure):
    _fields_ = [('kind', C.c_int), ('lower', C.c_size_t), ('upper', C.c_size_t), ('extensible', C.c_int)]

def cases():
    # kind, ASN.1 type, C range (integer or size), values.
    yield 'big', 'INTEGER', (0,0,0,0,0), [0,1,-1,127,128,-127,-128,255,-255,256,-256, -(1<<63),(1<<63)-1,1<<64,-(1<<64),(1<<160)+1,-(1<<160)]
    for low, high in [(0,7),(-3,8),(0,254),(0,255),(0,256),(0,65535),(0,65536),(0,1000000),(-(1<<63),(1<<63)-1)]:
        yield 'int', f'INTEGER ({low}..{high})', (low,1,high,1,0), sorted({low,high,low+(high-low)//2})
    yield 'int', 'INTEGER (-100..MAX)', (-100,1,0,0,0), [-100,-99,0,255,(1<<63)-1]
    yield 'int', 'INTEGER (MIN..100)', (0,0,100,1,0), [-1000,0,100]
    yield 'int', 'INTEGER (-3..8, ...)', (-3,1,8,1,1), [-1000,-3,8,9,1000]
    for kind, base, short in [('octets','OCTET STRING',2),('bits','BIT STRING',16)]:
        definitions = [('', (0,0,0,0), [0,1,short,127,128,16383,16384,16385,32768,49152,65536,65537]),
                       (' (SIZE (0))',(1,0,0,0),[0]),
                       (f' (SIZE ({short}))',(1,short,short,0),[short]),
                       (f' (SIZE ({short+1}))',(1,short+1,short+1,0),[short+1]),
                       (' (SIZE (1..16))',(1,1,16,0),[1,3,8,9,16]),
                       (' (SIZE (0..16))',(1,0,16,0),[0,1]),
                       (' (SIZE (1..65536))',(1,1,65536,0),[1,3,16384,65536]),
                       (' (SIZE (3..MAX))',(2,3,0,0),[3,128,16384]),
                       (' (SIZE (1..16, ...))',(1,1,16,1),[0,1,16,17,16384]),
                       (' (SIZE (65536))',(1,65536,65536,0),[65536])]
        for suffix, constraint, lengths in definitions:
            values = []
            for n in lengths:
                raw = bytes((i*37+0xA5)&255 for i in range((n+7)//8 if kind=='bits' else n))
                if kind=='bits' and n%8: raw=raw[:-1]+bytes([raw[-1] & (255 << (8-n%8))])
                values.append((raw,n) if kind=='bits' else raw)
            yield kind, base+suffix, constraint, values

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--build-dir',type=pathlib.Path,required=True)
    ap.add_argument('--write-vectors',action='store_true'); args=ap.parse_args()
    args.build_dir.mkdir(parents=True,exist_ok=True)
    so=args.build_dir.resolve()/'libc_per_oracle.so'
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-O2','-shared','-fPIC',
                    '-DASN1_MAX_STRING_BYTES=131072','-I'+str(ROOT/'include'),
                    *map(str,sorted((ROOT/'src/c_runtime').glob('*.c'))),'-o',str(so)],check=True)
    lib=C.CDLL(str(so)); libc=C.CDLL(None); libc.free.argtypes=[C.c_void_p]
    def fn(name,args):
        f=getattr(lib,'asn1_'+name); f.argtypes=args; f.restype=C.c_int; return f
    ptr=C.c_void_p; pptr=C.POINTER(ptr); sz=C.POINTER(C.c_size_t)
    create=fn('bit_writer_create',[pptr]); put=fn('bit_writer_put_bit',[ptr,C.c_int])
    take=fn('bit_writer_take',[ptr,pptr,sz]); destroy=lib.asn1_bit_writer_destroy; destroy.argtypes=[ptr]
    rc=fn('bit_reader_create',[pptr,View]); get=fn('bit_reader_get_bit',[ptr,C.POINTER(C.c_int)])
    rd=lib.asn1_bit_reader_destroy; rd.argtypes=[ptr]
    bi=fn('bigint_init_from_decimal',[C.POINTER(Big),C.c_char_p,C.c_int]); bf=lib.asn1_bigint_free; bf.argtypes=[C.POINTER(Big)]
    twos=fn('bigint_to_twos_complement',[C.POINTER(Big),pptr,sz])
    enc={}; dec={}
    for kind in ['big','int','bits','octets']:
        if kind=='big':
            enc[kind]=fn('per_encode_unbounded_int_ex',[ptr,C.c_int,C.POINTER(Big)])
            dec[kind]=fn('per_decode_unbounded_int_ex',[ptr,C.c_int,C.POINTER(Big)])
        elif kind=='int':
            enc[kind]=fn('per_encode_int_ex',[ptr,C.c_int,C.POINTER(IntRange),C.c_int64])
            dec[kind]=fn('per_decode_int_ex',[ptr,C.c_int,C.POINTER(IntRange),C.POINTER(C.c_int64)])
        elif kind=='bits':
            enc[kind]=fn('per_encode_bitstring_ex',[ptr,C.c_int,C.POINTER(Size),C.POINTER(Bits)])
            dec[kind]=fn('per_decode_bitstring_ex',[ptr,C.c_int,C.POINTER(Size),C.POINTER(Bits)])
        else:
            enc[kind]=fn('per_encode_octetstring_ex',[ptr,C.c_int,C.POINTER(Size),View])
            dec[kind]=fn('per_decode_octetstring_ex',[ptr,C.c_int,C.POINTER(Size),pptr,sz])
    bitfree=lib.asn1_bitstring_free; bitfree.argtypes=[C.POINTER(Bits)]
    definitions=list(cases())
    schema='Oracle DEFINITIONS ::= BEGIN\n'+'\n'.join(f'T{i} ::= SEQUENCE {{ prefix BOOLEAN, value {t}, tail BOOLEAN }}' for i,(_,t,_,_) in enumerate(definitions))+'\nEND'
    frozen=['/* Generated by scripts/validate_c_per_oracle.py; asn1tools '+asn1tools.__version__+'; do not edit. */']
    frozen.append('static const struct oracle_vector oracle_vectors[] = {')
    count=0
    for variant, codec in enumerate(['uper','per']):
        oracle=asn1tools.compile_string(schema,codec)
        for i,(kind,typ,params,values) in enumerate(definitions):
            if kind == 'int' and params[1] and not params[3]:
                # asn1tools 0.166.0 drops the lower bound with MAX (also
                # recorded by the project's pre-existing PER oracle suite).
                print(codec, typ, 'EXCLUDED: oracle MAX-bound deviation; X.691 11.7 vectors in C test')
                continue
            for value in values:
                length = value[1] if kind == 'bits' else len(value) if kind == 'octets' else 0
                if variant == 1 and kind in ('bits','octets') and length == 0 and params[0] == 1 and params[1] == 0 and params[2] != 0:
                    print(codec,typ,'EXCLUDED: oracle aligns absent zero-length content; X.691 11.9.3.3/Note 2')
                    continue
                if kind in ('bits','octets') and params[3] and not params[1] <= length <= params[2]:
                    if kind == 'bits' or length >= 16384:
                        print(codec,typ,length,'EXCLUDED: oracle lacks BIT STRING extension / extension fragment chain')
                        continue
                expected=oracle.encode(f'T{i}',{'prefix':True,'value':value,'tail':True})
                w=ptr(); assert create(C.byref(w))==0; assert put(w,1)==0
                constraint=IntRange(*params) if kind=='int' else Size(*params) if kind in ('bits','octets') else None
                big=Big(); data=None
                if kind=='big':
                    assert bi(C.byref(big),str(value).encode(),0)==0
                    status=enc[kind](w,variant,C.byref(big))
                elif kind=='int': status=enc[kind](w,variant,C.byref(constraint),value)
                else:
                    raw,n=value if kind=='bits' else (value,len(value))
                    data=(C.c_uint8*len(raw)).from_buffer_copy(raw)
                    bits=Bits(data,n,len(raw))
                    status=enc[kind](w,variant,C.byref(constraint),C.byref(bits) if kind=='bits' else View(data,len(raw)))
                assert status==0,(codec,typ,value if kind in ('int','big') else n,status)
                assert put(w,1)==0; out=ptr(); size=C.c_size_t(); assert take(w,C.byref(out),C.byref(size))==0
                actual=C.string_at(out,size.value); libc.free(out); destroy(w)
                assert actual==expected,(codec,typ,str(value)[:100],len(actual),len(expected),actual[:16].hex(),expected[:16].hex(),actual[-16:].hex(),expected[-16:].hex())
                assert oracle.decode(f'T{i}',actual)=={'prefix':True,'value':value,'tail':True}
                # C decode of oracle-produced bytes, followed by the trailing field.
                source=(C.c_uint8*len(expected)).from_buffer_copy(expected); r=ptr()
                assert rc(C.byref(r),View(source,len(expected)))==0
                flag=C.c_int(); assert get(r,C.byref(flag))==0 and flag.value==1
                if kind=='big':
                    result=Big(); assert dec[kind](r,variant,C.byref(result))==0
                    p=ptr(); length=C.c_size_t(); assert twos(C.byref(result),C.byref(p),C.byref(length))==0
                    assert int.from_bytes(C.string_at(p,length.value),'big',signed=True)==value
                    libc.free(p); bf(C.byref(result)); bf(C.byref(big))
                elif kind=='int':
                    result=C.c_int64(); assert dec[kind](r,variant,C.byref(constraint),C.byref(result))==0 and result.value==value
                elif kind=='bits':
                    result=Bits(); assert dec[kind](r,variant,C.byref(constraint),C.byref(result))==0
                    assert result.bit_length==n and C.string_at(result.bits,(n+7)//8)==raw
                    bitfree(C.byref(result))
                else:
                    p=ptr(); length=C.c_size_t(); assert dec[kind](r,variant,C.byref(constraint),C.byref(p),C.byref(length))==0
                    assert C.string_at(p,length.value)==raw; libc.free(p)
                assert get(r,C.byref(flag))==0 and flag.value==1,(codec,typ,'tail')
                rd(r); count+=1
                if len(expected)<=40:
                    k={'big':0,'int':1,'bits':2,'octets':3}[kind]
                    param=list(params)+[0]*(5-len(params))
                    input_text=str(value) if kind in ('int','big') else raw.hex()
                    bitlen=0 if kind in ('int','big') else n
                    numbers = ['INT64_MIN' if x == -(1<<63) else 'INT64_MAX' if x == (1<<63)-1 else str(x) for x in [k,variant,*param,bitlen]]
                    frozen.append('  {'+','.join(numbers)+',"'+input_text+'","'+expected.hex()+'"},')
    frozen.append('};\n')
    text='\n'.join(frozen)
    target=ROOT/'tests/c_lang/golden/per_oracle_vectors.h'
    if args.write_vectors:
        target.parent.mkdir(parents=True,exist_ok=True); target.write_text(text)
    else: assert target.read_text()==text,'frozen oracle vectors differ; review before updating'
    print(f'asn1tools {asn1tools.__version__}: {count} C encode/oracle-decode and oracle-encode/C decode cases PASS; short vectors {len(frozen)-3}')

if __name__=='__main__': main()
