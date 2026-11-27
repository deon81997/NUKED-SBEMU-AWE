import zipfile,os,sys,subprocess,shutil
zp, img = sys.argv[1:3]; d='/tmp/flop/got'; shutil.rmtree(d, True); os.makedirs(d)
subprocess.run(['mcopy','-s','-n','-i',img+'@@32256','::/NUKEDSB','::/SC55',d],capture_output=True)
def dos(p): return '/'.join((lambda b,e: b[:8]+('.'+e[:3] if e else ''))(*(x.partition('.')[::2])) for x in p.split('/'))
z=zipfile.ZipFile(zp); n=bad=miss=0
for i in z.infolist():
    if i.is_dir(): continue
    p=os.path.join(d,dos(i.filename)); n+=1
    if not os.path.exists(p): miss+=1; print('MISSING',p); continue
    if open(p,'rb').read()!=z.read(i): bad+=1; print('DIFF',p)
r=os.path.join(d,'SC55','ROMS'); print(n,'files',bad,'differ',miss,'missing; C:\\SC55\\ROMS has',len(os.listdir(r)) if os.path.isdir(r) else 0,'files')
