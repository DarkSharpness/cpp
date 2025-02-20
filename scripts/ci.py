import os
import sys
import uuid
class Fore:
    RESET = '\033[0m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN = '\033[36m'
    WHITE = '\033[37m'

def panic(msg : str) -> None:
    print(f'{Fore.RED}Fatal error: {Fore.RESET}{msg}', end=None)
    sys.exit(1)

script = f'{os.path.dirname(__file__)}/compile.bash'

if not os.path.exists(script):
    panic('The script file does not exist.')

random_file = f'/tmp/ci_compile_cmd_{uuid.uuid4().hex}.log'

if os.system(f'bash {script} __test__ -fsyntax-only g++ only > {random_file}'):
    panic('Failed to generate compile command.')

with open(random_file, 'r') as file:
    compile_cmd = file.read()

# remove the -o flag and replace the file name with a placeholder
# from g++ __test__ -o -fsyntax-only -> g++ {} -fsyntax-only
compile_cmd = 'ccache ' + compile_cmd.strip().replace('__test__', '{}').replace(' -o ', ' ')

for file in sys.argv[1:]:
    if os.system(compile_cmd.format(file) + ' 2>/dev/null >/dev/null'):
        panic('Failed to compile file: ' + file + '\n- ' + compile_cmd.format(file))
