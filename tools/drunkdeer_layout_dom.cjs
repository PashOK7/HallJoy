// Code-only source extraction. No screenshot, HID, vendor JS, or real browser profile.
const fs=require('fs'), path=require('path'), cp=require('child_process');
const {chromium}=require('playwright');
const crypto=require('crypto');
const root=path.resolve(__dirname,'..');
const source=path.join(root,'.local/research/layout-import-drunkdeer');
const css=fs.readFileSync(path.join(source,'index.BxlH6I_0.css'),'utf8');
const run=cp.spawnSync('py',[path.join(__dirname,'drunkdeer_layout_source.py'),'--json'],{encoding:'utf8'});
if(run.status!==0) throw Error(run.stderr);
const data=JSON.parse(run.stdout);
(async()=>{
 const browser=await chromium.launch({headless:true});
 try {
  const page=await browser.newPage({viewport:{width:1600,height:1200}});
  await page.route('**/*',r=>r.abort());
  await page.setContent('<style>'+css+'</style><style>html{font-size:100px!important}</style>');
  const result=await page.evaluate(data=>{
   const results={};
   for(const [model,rows] of Object.entries(data)){
    const root=document.createElement('div');
    root.className=(model==='A75_iso_uk'?'A75IOS_UK':model)+' kay_broad';
    root.setAttribute('data-v-f78eccaf','');
    for(const row of rows){
     const line=document.createElement('div');line.className='keystroke-list';line.setAttribute('data-v-f78eccaf','');
     for(const key of row){const el=document.createElement('div');el.className=key.className+' keystroke';el.setAttribute('data-v-f78eccaf','');line.append(el);}
     root.append(line);
    }
    document.body.append(root);
    const children=[...root.querySelectorAll('.keystroke')];
    const boxes=children.map((el,i)=>{const r=el.getBoundingClientRect(),s=getComputedStyle(el);return {...rows.flat()[i],x:r.x,y:r.y,w:r.width,h:r.height,background:s.backgroundImage};});
    const minX=Math.min(...boxes.map(x=>x.x)),minY=Math.min(...boxes.map(x=>x.y));
    boxes.forEach(b=>{b.x-=minX;b.y-=minY;});
    results[model]=boxes;root.remove();
   }
   return results;
  },data);
  const output={sources:{}};
  for(const name of ['index.CJWCGjvj.js','index.BxlH6I_0.css']) output.sources[name]=crypto.createHash('sha256').update(fs.readFileSync(path.join(source,name))).digest('hex');
  output.models=result;
  const dest=path.join(source,'dom-layouts.json');
  const bytes=JSON.stringify(output,null,2)+'\n';
  if(fs.existsSync(dest)){if(fs.readFileSync(dest,'utf8')!==bytes)throw Error('Refusing changed output');}
  else fs.writeFileSync(dest,bytes,{flag:'wx'});
  console.log('DRUNKDEER_DOM=PASS models='+Object.keys(result).length);
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
