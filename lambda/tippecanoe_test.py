from tippecanoe import split_key
import unittest

class TestTippecanoe(unittest.TestCase):
    def test_basic(self):
        root, ext = split_key("file.json")
        self.assertEqual(root,"file")
        self.assertEqual(ext,".json")

    def test_basic_gz(self):
        root, ext = split_key("file.csv.gz")
        self.assertEqual(root,"file")
        self.assertEqual(ext,".csv.gz")

    def test_nested_key(self):
        root, ext = split_key("dir/file.fgb")
        self.assertEqual(root,"dir/file")
        self.assertEqual(ext,".fgb")

    def test_nested_key_gz(self):
        root, ext = split_key("dir/file.geojsonseq.gz")
        self.assertEqual(root,"dir/file")
        self.assertEqual(ext,".geojsonseq.gz")

if __name__ == '__main__':
    unittest.main()