import { cli, Path } from "esmakefile";
import { C, platformCompiler } from "esmakefile-c";

cli((book, opts) => {
  const c = new C(platformCompiler(), {
    ...opts,
    book,
    cVersion: "C17",
    cxxVersion: "C++20",
  });

  book.add("all", []);

  const lib = c.addLibrary({
    name: "msgstream",
    version: "0.1.0",
    privateDefinitions: {
      MSGSTREAM_IS_COMPILING: "1",
    },
    src: ["src/msgstream.c"],
  });

  const test = Path.build("msgstream_test");
  c.addExecutable({
    name: "msgstream_test",
    src: ["test/msgstream_test.cpp"],
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
