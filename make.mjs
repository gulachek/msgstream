import { cli, Path } from "esmakefile";
import { writeFile, readFile } from "node:fs/promises";
import { createRequire } from "node:module";
import { errorCodes } from "./jslib/errorCodes.mjs";
import { Clang } from "./jslib/clang.mjs";
import { Ajv } from "ajv";
import { PkgConfig } from "espkg-config";

const require = createRequire(import.meta.url);
const nunjucks = require("nunjucks");

const ajv = new Ajv();
const stringArray = { type: "array", items: { type: "string" } };
const schema = {
  type: "object",
  properties: {
    cc: { type: "string" },
    cxx: { type: "string" },
    cppflags: { ...stringArray },
    cflags: { ...stringArray },
    cxxflags: { ...stringArray },
    ldflags: { ...stringArray },
    pkgConfigPaths: { ...stringArray, uniqueItems: true },
    libraryType: { type: "string", enum: ["static", "shared"] },
  },
  additionalProperties: false,
};

const validate = ajv.compile(schema);

const inputConfig = JSON.parse(await readFile("config.json", "utf8"));
if (!validate(inputConfig)) {
  console.error('The configuration file "config.json" has errors.');
  console.error(validate.errors);
  process.exit(1);
}

const config = {
  cc: "clang",
  cxx: "clang++",
  cppflags: [],
  cflags: [
    "-std=c17",
    "-fvisibility=hidden",
    '-DMSGSTREAM_API=__attribute__((visibility("default")))',
  ],
  cxxflags: ["-std=c++20"],
  ldflags: [],
  pkgConfigPaths: ["pkgconfig"],
  libraryType: "static",
  ...inputConfig, // override the defaults
};

const pkg = new PkgConfig({
  searchPaths: config.pkgConfigPaths,
});

function addRenderWithErrorCodes(make, dest, srcRaw) {
  const src = Path.src(srcRaw);
  make.add(dest, [src], async (args) => {
    const output = nunjucks.render(args.abs(Path.src(src)), { errorCodes });
    await writeFile(args.abs(dest), output, "utf8");
  });
}

const bt = "boost-unit_test_framework";
const { flags: btCflags } = await pkg.cflags([bt]);
const { flags: btLibs } = await pkg.libs([bt]);

cli((make) => {
  const include = Path.src("include");
  const genInclude = Path.build("include");
  const errcH = genInclude.join("msgstream/errc.h");
  const errcC = Path.build("errc.c");

  config.cppflags.unshift("-I", make.abs(include), "-I", make.abs(genInclude));

  const clang = new Clang(make, config);

  make.add("all", []);

  addRenderWithErrorCodes(make, errcH, "src/njx/errc.h");
  addRenderWithErrorCodes(make, errcC, "src/njx/errc.c");

  const msg = clang.compile("src/msgstream.c");
  const err = clang.compile(errcC);
  make.add(err, [errcH]);

  const lib = clang.link({
    name: "msgstream",
    runtime: "c",
    binaries: [msg, err],
    linkType: config.libraryType,
  });

  const test = clang.link({
    name: "msgstream_test",
    runtime: "c++",
    linkType: "executable",
    binaries: [
      clang.compile("test/msgstream_test.cpp", { extraFlags: btCflags }),
      lib,
    ],
    extraFlags: btLibs,
  });

  const runTest = Path.build("run-test");
  make.add(runTest, [test], (args) => {
    return args.spawn(args.abs(test));
  });

  make.add("all", [runTest, clang.compileCommands()]);
});
