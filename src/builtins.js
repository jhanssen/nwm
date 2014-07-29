nwm.on("layout", function(client) {
    var rect = nwm.screens[client.screen];
    console.log("got client for layout", client, rect);
    var clientRect;
    if (!client.movable) {
        client.move(0, 0);
        client.resize(rect.width, rect.height);
    } else {
        var x = client.rect.x;
        if (typeof client.rect.x === 'undefined')
            x = (rect.width - client.rect.width) / 2;
        var y = client.rect.y;
        if (typeof client.rect.y === 'undefined')
            y = (rect.height - client.rect.height) / 2;

        console.log("movable client", client, x, y);
        client.move(x, y);
    }
});
