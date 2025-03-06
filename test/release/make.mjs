import { cli } from "esmakefile";
import { Distribution } from "esmakefile-cmake";

cli((make) => {
  const test = new Distribution(make, {
    name: "test",
    version: "1.2.3",
  });

  const msgstream = test.findPackage("msgstream");

  const hdrSize = test.addTest({
    name: "header_size",
    src: ["header_size.c"],
    linkTo: [msgstream],
  });

  make.add("test", [hdrSize.run], () => {});
});
