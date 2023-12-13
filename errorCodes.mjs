const defs = [
  ["OK", "no error detected"],
  ["EOF", "end of file"],
];

export const errorCodes = defs.map((val, i) => {
  const [name, msg] = val;
  return { name: `MSGSTREAM_${name}`, value: i, msg };
});
