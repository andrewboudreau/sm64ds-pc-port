"""One-shot portsync3 seed step: for each object an unresolved-link list names, apply merge_rename's
class renames to the port source that object came from.

    python port/tools/portsync3_seed/classapply.py [--config DIR] [--unresolved FILE] [--apply]

--config defaults to the decomp root's config/ (port/tools/decomp_root.py); --unresolved defaults to
unresolved_first_links.txt beside this script. Without --apply it only prints what it would change.
"""
import sys,pathlib,re,collections,os,glob,argparse
_here=os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.dirname(_here))
import decomp_root
import merge_rename as M
_ap=argparse.ArgumentParser()
_ap.add_argument('--config',default=None)
_ap.add_argument('--unresolved',default=os.path.join(_here,'unresolved_first_links.txt'))
_ap.add_argument('--apply',action='store_true')
_a=_ap.parse_args()
rn=M.Renamer(pathlib.Path(_a.config or decomp_root.join(decomp_root.find(),'config')),None)
cm=rn.cmap
rows=[l.rstrip('\n') for l in open(_a.unresolved)]
need=collections.defaultdict(set)
for r in rows:
    s,_,o=r.partition('  <- ')
    m=re.search(r'\((\?[^)]*)\)$',s)
    dec=m.group(1) if m else s
    cls=None
    mm=re.match(r'\?\??[\w$]*@(\w+)@@',dec)
    if dec.startswith('??'):
        mm2=re.match(r'\?\?[0-9](\w+)@@',dec)
        if mm2: cls=mm2.group(1)
    elif mm: cls=mm.group(1)
    elif dec.startswith('@actorport_d16t_'):
        cls=re.match(r'@actorport_d16t_(\w+)@8',dec).group(1)
    if cls and cls in cm:
        for ob in o.split(','): need[ob.strip()].add(cls)
apply=_a.apply
for ob,cl in sorted(need.items()):
    stem=ob[:-4]  # drop .obj
    cands=[p for p in glob.glob(os.path.join(glob.escape(decomp_root.PORT_DIR),'**',stem),recursive=True) if not 'build' in p]
    print(ob,sorted(cl),'->',[os.path.relpath(c,decomp_root.PORT_TREE) for c in cands][:2])
    if apply:
        for c in cands[:1]:
            raw=open(c,encoding='utf-8',errors='replace',newline='').read()
            new=rn.rewrite(raw,'c',set(cl))
            if new!=raw: open(c,'w',encoding='utf-8',newline='').write(new); print('   changed')
