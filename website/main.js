// Подтягивает последний релиз с GitHub и обновляет ссылки на установщики.
// Если сети нет — остаются статические ссылки на страницу релизов.
(function () {
    "use strict";

    function fmtSize(bytes) {
        if (bytes > 1024 * 1024) return (bytes / 1024 / 1024).toFixed(1) + " МБ";
        return Math.round(bytes / 1024) + " КБ";
    }

    fetch("https://api.github.com/repos/I114rk/QTIV/releases/latest")
        .then(function (r) { return r.json(); })
        .then(function (rel) {
            var ver = document.getElementById("ver");
            if (ver && rel.tag_name) ver.textContent = rel.tag_name.replace(/^v/, "");

            document.querySelectorAll("[data-asset]").forEach(function (a) {
                var want = a.getAttribute("data-asset");
                var asset = (rel.assets || []).filter(function (x) {
                    return x.name && x.name.indexOf(want) !== -1;
                })[0];
                if (asset) {
                    a.href = asset.browser_download_url;
                    var row = a.closest("tr");
                    var size = row && row.querySelector(".size");
                    if (size) size.textContent = fmtSize(asset.size);
                    var name = row && row.querySelector(".mono");
                    if (name && name.textContent.indexOf("qtiv") === 0)
                        name.textContent = asset.name;
                }
            });

            var src = document.getElementById("srclink");
            if (src && rel.tag_name)
                src.href = "https://github.com/I114rk/QTIV/releases/tag/" + rel.tag_name;
        })
        .catch(function () { /* остаётся статика */ });
})();
