import { cli, Path } from "esmakefile";
import { C, platformCompiler } from "esmakefile-c";
import { writeFile } from "node:fs/promises";
import { createRequire } from "node:module";
import { errorCodes } from "./errorCodes.mjs";

const require = createRequire(import.meta.url);
const nunjucks = require("nunjucks");

function addRenderWithErrorCodes(book, dest, srcRaw) {
  const src = Path.src(srcRaw);
  const errs = Path.src("errorCodes.mjs");
  book.add(dest, [src, errs], async (args) => {
    const output = nunjucks.render(args.abs(Path.src(src)), { errorCodes });
    await writeFile(args.abs(dest), output, "utf8");
  });
}

cli((book, opts) => {
  const c = new C(platformCompiler(), {
    ...opts,
    book,
    cVersion: "C17",
    cxxVersion: "C++20",
  });

  book.add("all", []);

  const genInclude = Path.build("include");
  const errcH = genInclude.join("msgstream/errc.h");
  addRenderWithErrorCodes(book, errcH, "src/njx/errc.h");

  const errcC = Path.build("errc.c");
  addRenderWithErrorCodes(book, errcC, "src/njx/errc.c");

  const lib = c.addLibrary({
    name: "msgstream",
    version: "0.1.0",
    privateIncludes: [genInclude],
    privateDefinitions: {
      MSGSTREAM_IS_COMPILING: "1",
    },
    src: ["src/msgstream.c", errcC],
  });

  book.add(Path.build("src/msgstream.o"), errcH);

  book.add("msgstream", lib);

  const test = Path.build("msgstream_test");
  c.addExecutable({
    name: "msgstream_test",
    src: ["test/msgstream_test.cpp"],
    privateIncludes: [genInclude],
    link: [lib, "boost-unit_test_framework"],
  });

  const compileCommands = Path.build("compile_commands.json");
  c.addCompileCommands();

  const runTest = Path.build("run-test");
  book.add(runTest, [test], (args) => {
    return args.spawn(args.abs(test));
  });

  book.add("all", [runTest, compileCommands]);
});
