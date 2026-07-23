// nic z zewnatrz, nic co dzwoni do domu - tylko reveal-on-scroll dla sekcji
document.addEventListener("DOMContentLoaded", function () {
    var targets = document.querySelectorAll(".reveal");
    if (!("IntersectionObserver" in window)) {
        targets.forEach(function (el) { el.classList.add("in"); });
        return;
    }
    var io = new IntersectionObserver(
        function (entries) {
            entries.forEach(function (entry) {
                if (entry.isIntersecting) {
                    entry.target.classList.add("in");
                    io.unobserve(entry.target);
                }
            });
        },
        { threshold: 0.12 }
    );
    targets.forEach(function (el) { io.observe(el); });

    // zabezpieczenie - gdyby observer z jakiegos powodu nie zlapal jakiegos elementu
    // (dziwny layout, zmiana rozmiaru okna w trakcie), tresc i tak ma sie pokazac
    setTimeout(function () {
        targets.forEach(function (el) { el.classList.add("in"); });
    }, 2500);
});
