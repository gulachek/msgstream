import { cli, Path } from "esmakefile";
import { writeFile } from "node:fs/promises";
import { createRequire } from "node:module";
import { errorCodes } from "./jslib/errorCodes.mjs";
import { Distribution, addCompileCommands } from "esmakefile-cmake";

const require = createRequire(import.meta.url);
const nunjucks = require("nunjucks");

function addRenderWithErrorCodes(make, dest, srcRaw) {
  const src = Path.src(srcRaw);
  make.add(dest, [src], async (args) => {
    const output = nunjucks.render(args.abs(Path.src(src)), { errorCodes });
    await writeFile(args.abs(dest), output, "utf8");
  });
}

cli((make) => {
  const include = Path.src("include");
  const genInclude = Path.build("include");
  const errcH = genInclude.join("msgstream/errc.h");
  const errcC = Path.build("src/errc.c");

  make.add("all", []);

  addRenderWithErrorCodes(make, errcH, "src/njx/errc.h");
  addRenderWithErrorCodes(make, errcC, "src/njx/errc.c");

  const d = new Distribution(make, {
    name: "msgstream",
    version: "0.1.0",
    cStd: 11,
    cxxStd: 20,
  });

  const msg = d.addLibrary({
    name: "msgstream",
    src: ["src/msgstream.c", errcC],
    includeDirs: [include, genInclude],
  });

  // TODO - esmakefile-cmake should support obj for src lookup
  const err = Path.gen(errcC, { ext: ".o" });
  make.add(err, [errcH]);

  const gtest = d.findPackage("gtest_main");

  d.addTest({
    name: "msgstream_test",
    src: ["test/msgstream_test.cpp"],
    linkTo: [msg, gtest],
  });

  make.add("test", [d.test], () => {});

  const compileCommands = addCompileCommands(make, d);

  make.add("all", [d.test, compileCommands]);
});
