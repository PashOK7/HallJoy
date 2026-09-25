"""Regression for sparse CellData returned by the native Sheets API."""
import unittest
from check_keyboard_sheet_structure import audit

class SparseSheetTests(unittest.TestCase):
    def fixture(self):
        edge={'style':'SOLID_MEDIUM','colorStyle':{'rgbColor':{'red':.48235294,'green':.5372549,'blue':.60784316}}}
        cells=[]
        for j,v in enumerate(('Example','Model','Supported')):
            borders={'top':edge,'bottom':edge}
            if j==0:borders['left']=edge
            if j==2:borders['right']=edge
            c={'formattedValue':v,'userEnteredFormat':{'borders':borders}}
            if j==2:c['dataValidation']={'strict':True,'condition':{'type':'ONE_OF_LIST','values':[{'userEnteredValue':'Supported'}]}}
            cells.append(c)
        return {'sheets':[{'properties':{'title':'Main','sheetId':0,'gridProperties':{'rowCount':4}},
                'data':[{'rowData':[{}, {'values':cells}, {}, {'values':[{}]}],
                         'rowMetadata':[{'pixelSize':32},{'pixelSize':32},{'pixelSize':16},{'pixelSize':16}]}]}]}
    def test_omitted_separator_cells_are_empty(self):
        requests,issues,blocks=audit(self.fixture())
        self.assertEqual((requests,issues,blocks),([],[],1))
    def test_omitted_model_status_still_fails(self):
        s=self.fixture();s['sheets'][0]['data'][0]['rowData'][1]['values'].pop()
        with self.assertRaisesRegex(AssertionError,'Incomplete model row'):audit(s)

if __name__=='__main__':unittest.main()
