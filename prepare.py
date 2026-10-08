import pathlib, subprocess, shutil, json, sys
from PIL import Image, ImageDraw, ImageFont
root=pathlib.Path(__file__).resolve().parent
app=pathlib.Path(sys.argv[1]).resolve(); assets=app/'assets'; assets.mkdir(exist_ok=True)
def raw(name,img):
    img=img.convert('RGBA'); (assets/(name+'.rgba')).write_bytes(img.tobytes()); return img
raw('menu',Image.open(root/'media/menu.jpg').resize((960,540)))
raw('logo',Image.open(root/'media/logo.png').resize((720,540)))
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',64)
for name,text in [('new','New Game'),('settings','Settings'),('thanks','Thanks for using GTA6 BJB'),('back','Back'),('sound','Menu music'),('error','Unable to open media')]:
    im=Image.new('RGBA',(1000,100)); d=ImageDraw.Draw(im); f=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',90) if name in ('new','settings','back','sound') else font; d.text((500,50),text,font=f,fill='white',anchor='mm'); raw(name,im)
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
(assets/'timing.json').write_text(json.dumps(report,indent=2))
for f in (app/'src').iterdir():
    if f.is_file(): f.unlink()
for f in (root/'src').iterdir(): shutil.copy2(f,app/'src'/f.name)
p=app/'sce_sys/param.json'; param=json.loads(p.read_text()); param.update(titleId='PPSA99991',conceptId='99991',contentId='UP9000-PPSA99991_00-GTA6BJB000000000',contentVersion='01.000.002'); param['localizedParameters']['en-US']['titleName']='Grand Theft Auto 6'; p.write_text(json.dumps(param,indent=2)+'\n')
icon=Image.new('RGBA',(512,512),(0,0,0,255)); logo=Image.open(root/'media/logo.png').convert('RGBA'); logo.thumbnail((490,490)); icon.alpha_composite(logo,((512-logo.width)//2,(512-logo.height)//2)); icon.save(app/'sce_sys/icon0.png')
