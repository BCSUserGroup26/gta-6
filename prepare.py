import pathlib, subprocess, shutil, json, sys
from PIL import Image, ImageDraw, ImageFont
root=pathlib.Path(__file__).resolve().parent
app=pathlib.Path(sys.argv[1]).resolve(); assets=app/'assets'; assets.mkdir(exist_ok=True)
def raw(name,img):
    img=img.convert('RGBA'); (assets/(name+'.rgba')).write_bytes(img.tobytes()); return img
raw('menu',Image.open(root/'media/menu.jpg').resize((1920,1080),Image.Resampling.LANCZOS))
raw('logo',Image.open(root/'media/logo.png').resize((720,540)))
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',64)
gradient=Image.new('RGBA',(960,150)); gd=ImageDraw.Draw(gradient)
for y in range(150): gd.line((0,y,959,y),fill=(6,7,18,int(235*(y/149)**0.7)))
raw('shade',gradient)
for name,text in [('new','NEW GAME'),('settings','SETTINGS'),('thanks','Thanks for using GTA6 BJB'),('back','BACK'),('sound','MENU MUSIC'),('error','Unable to open media'),('hints','✕  SELECT     |     ↔  ↕  NAVIGATE')]:
    im=Image.new('RGBA',(1000,100)); d=ImageDraw.Draw(im)
    if name in ('new','settings','back','sound'):
        f=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',78)
        widths=[d.textlength(ch,font=f) for ch in text]; tracking=10; x=(1000-sum(widths)-tracking*(len(text)-1))/2
        for ch,w in zip(text,widths): d.text((x,50),ch,font=f,fill='white',anchor='lm');x+=w+tracking
    else:
        f=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',26) if name=='hints' else font
        d.text((500,50),text,font=f,fill='white',anchor='mm')
    raw(name,im)
report={}
for name in ['loading','loop','rickroll']:
    src=root/'media'/f'{name}.mp4'
    duration=float(subprocess.check_output(['ffprobe','-v','error','-show_entries','format=duration','-of','csv=p=0',str(src)],text=True)); report[name]=duration
    subprocess.run(['ffmpeg','-v','error','-y','-i',str(src),'-an','-vf','scale=640:360,fps=25','-c:v','mpeg1video','-b:v','2500k','-bf','0','-f','mpeg1video',str(assets/(name+'.m1v'))],check=True)
    probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_streams','-of','json',str(src)],text=True))
    args=['ffmpeg','-v','error','-y']
    args+=['-i',str(src)] if any(s['codec_type']=='audio' for s in probe['streams']) else ['-f','lavfi','-i','anullsrc=r=48000:cl=stereo','-t',str(duration)]
    subprocess.run(args+['-vn','-af','apad','-t',str(duration),'-ar','48000','-ac','2','-f','s16le',str(assets/(name+'.pcm'))],check=True)
subprocess.run(['ffmpeg','-v','error','-y','-i',str(root/'media/hover_theme.m4a'),'-ar','48000','-ac','2','-f','s16le',str(assets/'menu.pcm')],check=True)
for name in ['loading','loop','rickroll','menu']:
    pcm=assets/(name+'.pcm')
    if pcm.stat().st_size == 0 or pcm.stat().st_size % 4: raise SystemExit(f'Invalid stereo PCM: {name}')
for name in ['loading','loop','rickroll']:
    if (assets/(name+'.m1v')).stat().st_size < 32: raise SystemExit(f'Empty video: {name}')
(assets/'timing.json').write_text(json.dumps(report,indent=2))
for f in (app/'src').iterdir():
    if f.is_file(): f.unlink()
for f in (root/'src').iterdir(): shutil.copy2(f,app/'src'/f.name)
p=app/'sce_sys/param.json'; param=json.loads(p.read_text()); param.update(titleId='PPSA99991',conceptId='99991',contentId='UP9000-PPSA99991_00-GTA6BJB000000000',contentVersion='01.000.004'); param['localizedParameters']['en-US']['titleName']='Grand Theft Auto 6'; p.write_text(json.dumps(param,indent=2)+'\n')
icon=Image.new('RGBA',(512,512),(0,0,0,255)); logo=Image.open(root/'media/logo.png').convert('RGBA'); logo.thumbnail((490,490)); icon.alpha_composite(logo,((512-logo.width)//2,(512-logo.height)//2)); icon.save(app/'sce_sys/icon0.png')
