import nunjucks from "nunjucks";

export function njxRender(name, context) {
  return new Promise((resolve, reject) => {
    nunjucks.render(name, context, (err, res) => {
      if (err) {
        reject(err);
        return;
      }

      resolve(res);
    });
  });
}
