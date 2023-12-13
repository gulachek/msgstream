const defs = [
  ["OK", "no error detected"],
  ["EOF", "end of file"],
  ["NULL_ARG", "a null pointer was unexpectedly passed as an argument"],
  ["SMALL_BUF", "buffer is too small"],
  ["SMALL_HDR", "header size is too small"],
  ["BIG_HDR", "header size is too big"],
  ["HDR_SYNC", "header size mismatch between sent and received message"],
  ["BIG_MSG", "message size is too big"],
  ["SYS_READ_ERR", "read system call encountered an error"],
  ["SYS_WRITE_ERR", "write system call encountered an error"],
  ["TRUNC", "message truncated"],
];

export const errorCodes = defs.map((val, i) => {
  const [name, msg] = val;
  return { name: `MSGSTREAM_${name}`, value: i, msg };
});
