"""Exercise sketch preprocessing without compiling the SDK."""
import sys
import unittest
import tempfile
from unittest.mock import patch
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from sketch import parameter_end, without_defaults, add_prototypes


class SketchTests(unittest.TestCase):
    def test_defaults_and_nested_declarators(self):
        self.assertEqual(
            without_defaults('(int a = call(1, 2), void (*cb)(int, int) = nullptr)'),
            '(int a , void (*cb)(int, int) )')

    def test_literals(self):
        text = '(const char* text = "a,)=", char ch = \')\', int n = 0)'
        self.assertEqual(without_defaults(text), '(const char* text , char ch , int n )')
        self.assertEqual(parameter_end(text + " { body(); }", 0), len(text) - 1)

    def test_no_defaults(self):
        text = '(const char* text, void (*cb)(int, int))'
        self.assertEqual(without_defaults(text), text)

    def test_static_prototype_keeps_internal_linkage(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "test.ii"
            source.write_text('''# 1 "test.ino"
void setup(void) {}
static bool helper(int n) { return n; }
''')
            tags = '''helper	test.ii	/^static/;"	line:3	returntype:bool	signature:(int n)
'''
            with patch("sketch.subprocess.check_output", return_value=tags):
                add_prototypes(source, "ctags")
            self.assertIn("static bool helper(int n);", source.read_text())

    def test_unterminated(self):
        with self.assertRaises(ValueError):
            parameter_end('(int a', 0)


if __name__ == "__main__":
    unittest.main()
