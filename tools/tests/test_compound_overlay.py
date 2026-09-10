"""Parse production overlay JS and execute its shape path without a browser."""
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]


class CompoundOverlayTests(unittest.TestCase):
    def test_javascript_and_shared_outline(self):
        source = (ROOT/'src/HallJoyProject/HallJoy/overlay_server.cpp').read_text(encoding='utf-8-sig')
        html = ''.join(re.findall(r'R"HTML\((.*?)\)HTML"',source,re.S))
        scripts = re.findall(r'<script[^>]*>(.*?)</script>',html,re.S)
        self.assertTrue(scripts)
        for script in scripts:
            run = subprocess.run(['node','--check'],input=script,text=True,capture_output=True)
            self.assertEqual(run.returncode,0,run.stderr)
        function = re.search(r'function keyPath\(.*?\n}',html,re.S).group()
        test = r'''
const assert=require('assert');
let points=[],curves=[],rectangle=false;
function rr2(){rectangle=true;}
const g={beginPath(){points=[];curves=[]},moveTo(x,y){points.push([x,y])},
lineTo(x,y){points.push([x,y])},quadraticCurveTo(...p){curves.push(p)},closePath(){}};
keyPath(g,0,0,66,86,7,12,40);
assert.equal(curves.length,6);
assert.deepStrictEqual(curves.map(p=>p.slice(0,2)),[[0,0],[66,0],[66,86],[12,86],[12,40],[0,40]]);
assert.deepStrictEqual(curves[4],[12,40,6,40]); // Concave corner, bounded by short arm.
keyPath(g,10,20,132,172,7,24,80);
assert.deepStrictEqual(curves.map(p=>p.slice(0,2)),[[10,20],[142,20],[142,192],[34,192],[34,100],[10,100]]);
for(const nw of [1,12,65,70])for(const ny of [1,40,85,90]){
  keyPath(g,0,0,66,86,7,nw,ny);
  assert(points.concat(curves).flat().every(Number.isFinite));
  for(const p of points.concat(curves))for(let i=0;i<p.length;i++)assert(p[i]>=0&&p[i]<=(i%2?86:66));
}
keyPath(g,0,0,66,86,7);assert(rectangle);
'''
        run = subprocess.run(['node','-e',function+test],text=True,capture_output=True)
        self.assertEqual(run.returncode,0,run.stderr)


if __name__ == '__main__':
    unittest.main()
