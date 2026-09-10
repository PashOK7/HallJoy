"""Dependency-free reader of reviewed, source-locked manufacturer extractions."""
from pathlib import Path
import hashlib
import layout_import as common

ROOT=Path(__file__).resolve().parents[1]

def prepare(model,root=ROOT):
    def confined(relative):
        path=(root/relative).resolve()
        common.require(path.is_relative_to(root.resolve()),'Report/source escapes repository')
        return path
    report,sha=common.read_json(confined(model['source']))
    common.require(sha==model['sha256'],'Reviewed report changed')
    for field in ('id','model','variant'):
        common.require(report[field]==model[field],'Report identity mismatch')
    common.require(len(report['keys'])==model['count'],'Report key count mismatch')
    common.require(report['identity']['products']==model['products'],'Report selectors mismatch')
    common.require(report['brand']==model['brand'],'Report brand mismatch')
    for source in report['sources']:
        path=confined(source['path'])
        common.require(path.stat().st_size<=8*1024*1024,'Source size limit exceeded')
        common.require(hashlib.sha256(path.read_bytes()).hexdigest()==source['sha256'],'Manufacturer source changed')
    return report
