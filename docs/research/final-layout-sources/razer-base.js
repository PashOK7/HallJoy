(function () {
    const enableLog = localStorage.getItem('enableLog') === 'true';
    const isDarkMode = window.matchMedia('(prefers-color-scheme: dark)').matches;
    const color = isDarkMode ? '#fff' : '#000';
    // https://www.figma.com/design/kYlBwNFbcnGNpEIlShrZaV/-Maryann--Common-UI?node-id=1899-4226&t=krqTuziVnM6LxDYk-0
    console.log([
        '%cSTOP!',
        '%cThis tool is for developers only. If someone asks you to change settings to unlock hidden features or access another person\'s account, it is likely a scam that could put your Razer account at risk. Always verify information through official Razer sources.',
    ].join('\n'),
        'font-family: RazerF5;font-weight: 400;font-size: 48px;line-height: 48px;color: #FD4949;',
        `font-family: Roboto;font-style: normal;font-weight: 400;font-size: 18px;line-height: 21px;color: ${color};`
    );
    if (!enableLog && location.hostname === 'synapse.razer.com') {
        const noop = function () { };
        Object.getOwnPropertyNames(console).forEach(method => {
            if (typeof console[method] === 'function') {
                console[method] = noop;
            }
        });
        // const disableDevTool = new Function(`let devtoolsOpen = false;
        // const threshold = 160;
        // const onOpenDevTools = () => {
        //     // window.close();
        //     // window.location.href = 'about:blank';
        // };
        // window.addEventListener('resize', () => {
        //     const widthThreshold = window.outerWidth - window.innerWidth > threshold;
        //     const heightThreshold = window.outerHeight - window.innerHeight > threshold;
        //     if (widthThreshold || heightThreshold) {
        //         if (!devtoolsOpen) {
        //             devtoolsOpen = true;
        //             onOpenDevTools();
        //         }
        //     } else {
        //         devtoolsOpen = false;
        //     }
        // });
        // const isDevToolsOpenByDebugger = (threshold = 100) => {
        //     const start = new Date().getTime();
        //     debugger;
        //     const end = new Date().getTime();
        //     (end - start > threshold) && (devtoolsOpen = true, onOpenDevTools()) || (devtoolsOpen = false);
        // };
        // setInterval(isDevToolsOpenByDebugger, 500);
        // `);

        // disableDevTool();
    }
})();