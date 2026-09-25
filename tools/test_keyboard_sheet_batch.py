import copy
import json
import unittest
from check_keyboard_sheet_structure import audit
from plan_keyboard_sheet_batch import plan


class SheetBatchTests(unittest.TestCase):
    def fixture(self):
        rows = []
        for vals in [('Brand', 'Model', 'Status'), ('Alpha', 'M2', 'Not investigated'),
                     ('Alpha', 'M10', 'Not investigated'), ('', '', ''), ('Omega', 'Z1', 'Not investigated'), ('', '', '')]:
            cells = [{'formattedValue': v, 'userEnteredValue': {'stringValue': v},
                      'userEnteredFormat': {}} if v else {} for v in vals]
            if vals[2] == 'Not investigated':
                cells[2]['dataValidation'] = dict(strict=True, condition=dict(type='ONE_OF_LIST',
                    values=[{'userEnteredValue': x} for x in ('Not investigated', 'Implemented; awaiting hardware testing')]))
            rows.append({'values': cells})
        src = {'sheets': [{'properties': dict(sheetId=0, title='Main', gridProperties=dict(rowCount=6)),
            'data': [{'rowData': rows, 'rowMetadata': [{'pixelSize': n} for n in (32, 32, 32, 16, 32, 16)]}]}]}
        for req in audit(src)[0]:
            r = req['repeatCell']; area = r['range']
            rows[area['startRowIndex']]['values'][area['startColumnIndex']].setdefault('userEnteredFormat', {})['borders'] = r['cell']['userEnteredFormat']['borders']
        return src

    def test_insert_natural_order_new_brands_and_preserve_source(self):
        src = self.fixture(); old = copy.deepcopy(src)
        intents = [dict(brand=b, model=m, status='Implemented; awaiting hardware testing') for b, m in
                   [('Alpha', 'M3'), ('Beta', 'B1'), ('Aardvark', 'A1'), ('Zulu', 'Z1')]]
        result = plan(src, intents)
        self.assertEqual(src, old)
        self.assertEqual([(r['brand'], r['model']) for r in result['expected_models']],
                         [('Aardvark', 'A1'), ('Alpha', 'M2'), ('Alpha', 'M3'), ('Alpha', 'M10'),
                          ('Beta', 'B1'), ('Omega', 'Z1'), ('Zulu', 'Z1')])
        self.assertEqual(result['brand_blocks'], 5)
        self.assertEqual([i for i in result['original_row_indexes'] if i is not None], list(range(6)))

    def test_explicit_rename_and_status_only(self):
        result = plan(self.fixture(), [dict(brand='Omega', model='Z1 HE', status='Implemented; awaiting hardware testing')],
                      [dict(brand='Omega', **{'from': 'Z1', 'to': 'Z1 HE'})])
        value_requests = [r for r in result['requests'] if 'updateCells' in r]
        self.assertEqual(len(value_requests), 2)
        self.assertTrue(all(r['updateCells']['fields'] == 'userEnteredValue' for r in value_requests))
        self.assertEqual(result['expected_models'][-1]['status'], 'Implemented; awaiting hardware testing')

    def test_unsafe_plans_rejected(self):
        x = dict(brand='Alpha', model='M2', status='Implemented; awaiting hardware testing')
        for intents in ([x, x], [{**x, 'status': 'invalid'}]):
            with self.assertRaises(ValueError): plan(self.fixture(), intents)
        with self.assertRaises(ValueError):
            plan(self.fixture(), [], [dict(brand='Alpha', **{'from': 'M2', 'to': 'M10'})])

    def test_interned_formats_are_detached(self):
        src = self.fixture(); sheet = src['sheets'][0]; grid = sheet['data'][0]
        dictionary, indexes = [], {}
        def intern(v):
            key = json.dumps(v, sort_keys=True)
            if key not in indexes:
                indexes[key] = len(dictionary); dictionary.append(v)
            return indexes[key]
        packed = dict(sheet={k:v for k,v in sheet.items() if k != 'data'},
                      grid={k:v for k,v in grid.items() if k != 'rowData'}, dict=dictionary,
                      rows=[[{k:intern(v) for k,v in c.items()} for c in r['values']] for r in grid['rowData']])
        intents = [dict(brand='Beta', model='B1', status='Implemented; awaiting hardware testing'), dict(brand='Beta', model='B2', status='Implemented; awaiting hardware testing')]
        self.assertEqual(plan(src, intents), plan(packed, intents))


if __name__ == '__main__':
    unittest.main()
