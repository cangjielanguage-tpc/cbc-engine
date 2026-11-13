# asm-generator

Generates the `.asm` images run by the engine tests.
The output is committed under `../resources/generated-asm/`,
so the script is a maintenance tool: run it by hand and review the diff.

```bash
python3 test/asm-generator/asm-generator.py arith float-arith
```