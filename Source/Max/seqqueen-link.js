// seqqueen-link.js: finds Seq Queen FX on this device's track and points one
// live.remote~ at each effect's "Punch" parameter (outlets 0-9, in Seq Queen FX
// order). Outlet 10 sets the status line. Re-links whenever the track's devices
// change, so dropping the FX in after this device just works.
//
// This only runs when devices change, never per note, so the slow JS thread
// doesn't touch timing: the punches themselves go seqqueen.notes -> live.remote~.

autowatch = 1;
inlets = 1;
outlets = 11;

var FX = ["Tape Stop", "Delay Throw", "Filter", "Stutter", "Reverb",
          "Bitcrush", "Reverse", "Gate", "High-Pass", "Pitch"];

var watcher = null;
var pending = null;

// live.thisdevice bangs once the device is ready for the Live API.
function bang()
{
    if (watcher === null)
    {
        watcher = new LiveAPI (onDevicesChanged, "this_device canonical_parent");
        watcher.property = "devices";
    }
    link();
}

function onDevicesChanged (args)
{
    // Live doesn't allow acting inside its own notification; do it just after.
    if (args && args[0] === "devices")
    {
        if (pending === null)
            pending = new Task (function () { pending = null; link(); });
        pending.schedule (0);
    }
}

function asText (value)
{
    return (value instanceof Array) ? value.join (" ") : String (value);
}

function link()
{
    var ids = [];
    for (var f = 0; f < FX.length; f++)
        ids.push (0);

    var found = false;
    var track = new LiveAPI ("this_device canonical_parent");
    var count = track.getcount ("devices");

    for (var d = 0; d < count && ! found; d++)
    {
        var path = "this_device canonical_parent devices " + d;
        var device = new LiveAPI (path);
        var name = asText (device.get ("name")) + " " + asText (device.get ("class_display_name"));
        if (name.indexOf ("Seq Queen FX") < 0)
            continue;

        found = true;
        var numParams = device.getcount ("parameters");
        for (var p = 0; p < numParams; p++)
        {
            var param = new LiveAPI (path + " parameters " + p);
            var paramName = asText (param.get ("name"));
            for (var g = 0; g < FX.length; g++)
                if (paramName === FX[g] + " Punch")
                    ids[g] = Number (param.id);
        }
    }

    var linked = 0;
    for (var k = 0; k < FX.length; k++)
    {
        outlet (k, "id", ids[k]);
        if (ids[k] !== 0)
            linked++;
    }

    if (! found)
        outlet (10, "set", "FX: add Seq Queen FX after your instrument to use C0-A0");
    else if (linked === FX.length)
        outlet (10, "set", "FX: linked to Seq Queen FX (C0-A0)");
    else
        outlet (10, "set", "FX: found Seq Queen FX, but only " + linked + " of 10 punches are visible to Live");
}
