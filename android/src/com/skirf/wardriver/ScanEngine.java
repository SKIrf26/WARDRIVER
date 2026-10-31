package com.skirf.wardriver;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.location.GnssStatus;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.SystemClock;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.telephony.CellIdentityGsm;
import android.telephony.CellIdentityLte;
import android.telephony.CellIdentityWcdma;
import android.telephony.CellInfo;
import android.telephony.CellInfoGsm;
import android.telephony.CellInfoLte;
import android.telephony.CellInfoWcdma;
import android.telephony.TelephonyManager;
import android.util.SparseArray;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;

/**
 * The whole wardrive: GPS, Wi-Fi, Bluetooth LE, classic Bluetooth and cell towers, feeding one CSV log.
 * Everything here runs on the main thread except file writes (CsvLog) and network (Uploader).
 */
@SuppressLint("MissingPermission")
public final class ScanEngine {
    private static ScanEngine inst;
    public static synchronized ScanEngine get() {
        if (inst == null) inst = new ScanEngine();
        return inst;
    }

    private Context ctx;
    public final CsvLog csv = new CsvLog();

    // ---------------- run state ----------------
    public boolean running;
    public long startMs, stopMs;
    public final int[] uniq = new int[4];        // W, B, C, T
    public final int[] bands = new int[3];       // 2.4, 5, 6 GHz
    public final int[] sec = new int[5];         // open..wpa3
    public int newEver;                          // never seen on this phone before
    public int heardNoFix;                       // heard but not logged (no GPS fix yet)
    public int openNets;
    public double distanceM;
    public int strongestRssi = -200;
    public String strongestName = "";
    public static final int BIN_MAX = 720;       // 12 h of minutes
    public final int[][] bins = new int[BIN_MAX][4];
    private final ArrayDeque<Sighting> feed = new ArrayDeque<Sighting>();
    private final HashMap<Long, long[]> seen = new HashMap<Long, long[]>();   // key -> {bestRssi, lastLogMs}
    public List<java.io.File> lastRunFiles = new ArrayList<java.io.File>();
    public int lastRunRows;
    public long lastRunMs;

    // map
    public double[] trackLat = new double[1024], trackLon = new double[1024];
    public int trackN;
    public double[] dotLat = new double[2048], dotLon = new double[2048];
    public byte[] dotType = new byte[2048];
    public int dotN;
    public static final int DOT_MAX = 60000;

    // all-time unique list (survives app restarts)
    private final HashSet<Long> allTime = new HashSet<Long>();
    private volatile boolean allTimeLoaded;
    public int allTimeCount() { return allTime.size(); }

    // ---------------- GPS ----------------
    private LocationManager lm;
    private boolean gpsOn;
    private int uiRefs;
    public Location loc;            // last good location (GPS, or network when allowed)
    public long locAtMs;            // elapsedRealtime of loc
    public boolean locIsGps;
    public int satsInView, satsUsed;
    public int satN;
    public int[] satSvid = new int[96], satCons = new int[96];
    public float[] satCn0 = new float[96], satElev = new float[96], satAz = new float[96];
    public boolean[] satUsed = new boolean[96];
    public long gpsStartedAt;
    public long firstFixMs;          // time to first fix this session

    // ---------------- Wi-Fi ----------------
    private WifiManager wifi;
    public int wifiAsked, wifiAccepted, wifiRefused, wifiResults;
    public long wifiLastResultsAt;   // elapsedRealtime
    public String wifiState = "";
    private final ArrayDeque<Long> acceptedTimes = new ArrayDeque<Long>();
    public int wifiPerTwoMin;
    private boolean wifiReg;

    // ---------------- Bluetooth ----------------
    private BluetoothAdapter bt;
    private BluetoothLeScanner le;
    public String bleState = "", btState = "", cellState = "";
    public int bleResults, btResults, cellResults;
    private boolean bleOn, btReg, btWanted;
    private long bleStartedAt;

    // ---------------- cell ----------------
    private TelephonyManager tm;

    private PowerManager.WakeLock wake;

    // ======================================================================
    //  UI hooks: keep GPS running while the app is open so the fix is ready
    // ======================================================================
    public void uiAttach(Context c) {
        ctx = c.getApplicationContext();
        uiRefs++;
        ensureGps();
        loadAllTime();
    }

    public void uiDetach() {
        if (uiRefs > 0) uiRefs--;
        if (uiRefs == 0 && !running) stopGps();
    }

    public static boolean has(Context c, String perm) {
        return c.checkSelfPermission(perm) == PackageManager.PERMISSION_GRANTED;
    }

    // ======================================================================
    //  Start / stop
    // ======================================================================
    public void start(Context c) {
        if (running) return;
        ctx = c.getApplicationContext();
        resetRun();
        running = true;
        startMs = System.currentTimeMillis();
        loadAllTime();
        csv.start(Prefs.s(Prefs.PREFIX));
        ensureGps();
        if (Prefs.b(Prefs.WAKELOCK)) {
            PowerManager pm = (PowerManager) ctx.getSystemService(Context.POWER_SERVICE);
            if (pm != null) {
                wake = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "skirf:wardrive");
                wake.setReferenceCounted(false);
                wake.acquire(12 * 3600 * 1000L);
            }
        }
        if (Prefs.b(Prefs.WIFI)) startWifi(); else wifiState = "Off in settings";
        if (Prefs.b(Prefs.BLE)) startBle(); else bleState = "Off in settings";
        if (Prefs.b(Prefs.BT)) startBt(); else btState = "Off in settings";
        if (Prefs.b(Prefs.CELL)) startCell(); else cellState = "Off in settings";
        App.main.postDelayed(tick, 1000);
    }

    public void stop() {
        if (!running) return;
        running = false;
        stopMs = System.currentTimeMillis();
        App.main.removeCallbacks(tick);
        App.main.removeCallbacks(wifiTick);
        App.main.removeCallbacks(cellTick);
        App.main.removeCallbacks(bleRestart);
        App.main.removeCallbacks(btRestart);
        stopWifi();
        stopBle();
        stopBt();
        if (wake != null) { try { wake.release(); } catch (Throwable ignored) { } wake = null; }
        if (uiRefs == 0) stopGps();
        lastRunMs = stopMs - startMs;
        saveAllTime();
        csv.stop(new Runnable() {
            @Override public void run() {
                lastRunFiles = csv.files();
                lastRunRows = csv.rows();
                RunEnd.finished(ctx, lastRunFiles, lastRunRows, lastRunMs);
            }
        });
    }

    public void emergencyFlush() { csv.emergencyFlush(); }

    private void resetRun() {
        for (int i = 0; i < 4; i++) uniq[i] = 0;
        bands[0] = bands[1] = bands[2] = 0;
        for (int i = 0; i < 5; i++) sec[i] = 0;
        for (int[] b : bins) { b[0] = b[1] = b[2] = b[3] = 0; }
        newEver = heardNoFix = openNets = 0;
        distanceM = 0;
        strongestRssi = -200;
        strongestName = "";
        synchronized (feed) { feed.clear(); }
        seen.clear();
        trackN = 0;
        dotN = 0;
        wifiAsked = wifiAccepted = wifiRefused = wifiResults = 0;
        acceptedTimes.clear();
        bleResults = btResults = cellResults = 0;
        wifiState = bleState = btState = cellState = "";
    }

    /** Elapsed run time in ms. */
    public long elapsed() {
        if (startMs == 0) return 0;
        return (running ? System.currentTimeMillis() : stopMs) - startMs;
    }

    public int totalUnique() { return uniq[0] + uniq[1] + uniq[2] + uniq[3]; }

    public List<Sighting> feed(int max) {
        ArrayList<Sighting> l = new ArrayList<Sighting>();
        synchronized (feed) {
            for (Sighting s : feed) { if (l.size() >= max) break; l.add(s); }
        }
        return l;
    }

    /** Finds in each of the last n minutes, oldest first: [minute][type]. */
    public int[][] lastMinutes(int n) {
        int[][] r = new int[n][4];
        int cur = (int) (elapsed() / 60000);
        for (int i = 0; i < n; i++) {
            int m = cur - (n - 1 - i);
            if (m < 0 || m >= BIN_MAX) continue;
            System.arraycopy(bins[m], 0, r[i], 0, 4);
        }
        return r;
    }

    public int lastMinuteCount() {
        int cur = (int) (elapsed() / 60000);
        if (cur < 0 || cur >= BIN_MAX) return 0;
        int[] b = bins[cur];
        return b[0] + b[1] + b[2] + b[3];
    }

    // 1 Hz housekeeping
    private final Runnable tick = new Runnable() {
        @Override public void run() {
            if (!running) return;
            // Wi-Fi throttle window
            long now = SystemClock.elapsedRealtime();
            while (!acceptedTimes.isEmpty() && now - acceptedTimes.peekFirst() > 120000) acceptedTimes.pollFirst();
            wifiPerTwoMin = acceptedTimes.size();
            App.main.postDelayed(this, 1000);
        }
    };

    // ======================================================================
    //  GPS
    // ======================================================================
    private void ensureGps() {
        if (gpsOn || ctx == null) return;
        if (!has(ctx, Manifest.permission.ACCESS_FINE_LOCATION)) return;
        lm = (LocationManager) ctx.getSystemService(Context.LOCATION_SERVICE);
        if (lm == null) return;
        int rate = Math.max(500, Prefs.i(Prefs.GPS_RATE));
        try {
            if (lm.getProvider(LocationManager.GPS_PROVIDER) != null)
                lm.requestLocationUpdates(LocationManager.GPS_PROVIDER, rate, 0f, gpsListener, App.main.getLooper());
            if (Prefs.b(Prefs.NET_LOC) && lm.getProvider(LocationManager.NETWORK_PROVIDER) != null)
                lm.requestLocationUpdates(LocationManager.NETWORK_PROVIDER, 5000, 0f, netListener, App.main.getLooper());
            lm.registerGnssStatusCallback(gnss, App.main);
            gpsOn = true;
            gpsStartedAt = SystemClock.elapsedRealtime();
            firstFixMs = 0;
        } catch (Throwable t) {
            gpsOn = false;
        }
    }

    private void stopGps() {
        if (!gpsOn || lm == null) return;
        try { lm.removeUpdates(gpsListener); } catch (Throwable ignored) { }
        try { lm.removeUpdates(netListener); } catch (Throwable ignored) { }
        try { lm.unregisterGnssStatusCallback(gnss); } catch (Throwable ignored) { }
        gpsOn = false;
        satN = satsInView = satsUsed = 0;
    }

    /** Restart GPS listening (after a settings change). */
    public void restartGps() {
        stopGps();
        ensureGps();
    }

    public boolean locationServicesOn() {
        if (ctx == null) return true;
        LocationManager l = (LocationManager) ctx.getSystemService(Context.LOCATION_SERVICE);
        if (l == null) return false;
        if (Build.VERSION.SDK_INT >= 28) return l.isLocationEnabled();
        return l.isProviderEnabled(LocationManager.GPS_PROVIDER) || l.isProviderEnabled(LocationManager.NETWORK_PROVIDER);
    }

    public long fixAge() {
        if (loc == null) return Long.MAX_VALUE;
        return SystemClock.elapsedRealtime() - locAtMs;
    }

    /** A current position: recent enough to stamp on a row. */
    public boolean hasFix() {
        long limit = Math.max(4000, Prefs.i(Prefs.GPS_RATE) * 3L);
        if (!locIsGps) limit = 15000;
        return loc != null && fixAge() <= limit;
    }

    /** A current position that also meets the accuracy setting. */
    public boolean goodFix() {
        if (!hasFix()) return false;
        int max = Prefs.i(Prefs.MAX_ACC);
        return max <= 0 || !loc.hasAccuracy() || loc.getAccuracy() <= max;
    }

    private void onLocation(Location l, boolean gps) {
        if (l == null) return;
        if (!gps && locIsGps && fixAge() < 10000) return;   // GPS wins while it's fresh
        if (gps && firstFixMs == 0) firstFixMs = SystemClock.elapsedRealtime() - gpsStartedAt;
        Location prev = loc;
        loc = l;
        locAtMs = SystemClock.elapsedRealtime();
        locIsGps = gps;
        if (running && (!l.hasAccuracy() || l.getAccuracy() <= 60)) {
            if (trackN == 0) {
                addTrack(l.getLatitude(), l.getLongitude());
            } else {
                float[] d = new float[1];
                Location.distanceBetween(trackLat[trackN - 1], trackLon[trackN - 1], l.getLatitude(), l.getLongitude(), d);
                if (d[0] >= 5000) {
                    addTrack(l.getLatitude(), l.getLongitude());   // jump (bad first fix): restart the line, don't count it
                } else if (d[0] >= 8) {
                    distanceM += d[0];
                    addTrack(l.getLatitude(), l.getLongitude());
                }
            }
        }
    }

    private void addTrack(double lat, double lon) {
        if (trackN == trackLat.length) {
            trackLat = java.util.Arrays.copyOf(trackLat, trackN * 2);
            trackLon = java.util.Arrays.copyOf(trackLon, trackN * 2);
        }
        trackLat[trackN] = lat;
        trackLon[trackN] = lon;
        trackN++;
    }

    private final LocationListener gpsListener = new LocationListener() {
        @Override public void onLocationChanged(Location l) { onLocation(l, true); }
        @Override public void onStatusChanged(String p, int s, Bundle b) { }
        @Override public void onProviderEnabled(String p) { }
        @Override public void onProviderDisabled(String p) { }
    };
    private final LocationListener netListener = new LocationListener() {
        @Override public void onLocationChanged(Location l) { onLocation(l, false); }
        @Override public void onStatusChanged(String p, int s, Bundle b) { }
        @Override public void onProviderEnabled(String p) { }
        @Override public void onProviderDisabled(String p) { }
    };

    private final GnssStatus.Callback gnss = new GnssStatus.Callback() {
        @Override public void onSatelliteStatusChanged(GnssStatus s) {
            int n = Math.min(s.getSatelliteCount(), satSvid.length);
            int used = 0;
            for (int i = 0; i < n; i++) {
                satSvid[i] = s.getSvid(i);
                satCons[i] = s.getConstellationType(i);
                satCn0[i] = s.getCn0DbHz(i);
                satElev[i] = s.getElevationDegrees(i);
                satAz[i] = s.getAzimuthDegrees(i);
                satUsed[i] = s.usedInFix(i);
                if (satUsed[i]) used++;
            }
            satN = n;
            satsInView = n;
            satsUsed = used;
        }
        @Override public void onStopped() { satN = satsInView = satsUsed = 0; }
    };

    // ======================================================================
    //  The one place every sighting goes through
    // ======================================================================
    private void handle(Sighting s) {
        if (!running) return;
        int min = Prefs.i(Prefs.MIN_RSSI);
        if (min > -127 && s.rssi < min && s.type != Sighting.CELL) return;
        long k = Sighting.key(s.type, s.mac);
        long[] st = seen.get(k);
        boolean first = st == null;

        if (Prefs.b(Prefs.NEED_FIX) && !goodFix()) {
            if (first) heardNoFix++;
            return;   // not counted until it can be logged with a position
        }

        long now = System.currentTimeMillis();
        int mode = Prefs.i(Prefs.LOG_MODE);
        boolean log;
        if (first) log = true;
        else if (mode == 1) log = s.rssi >= st[0] + 6;
        else if (mode == 2) log = now - st[1] >= 10000;
        else log = false;
        if (!log) {
            if (st != null && s.rssi > st[0]) st[0] = s.rssi;
            return;
        }
        if (first) { st = new long[]{s.rssi, now}; seen.put(k, st); }
        else { st[0] = Math.max(st[0], s.rssi); st[1] = now; }

        double lat = 0, lon = 0, alt = 0, acc = 0;
        if (loc != null && hasFix()) {
            lat = loc.getLatitude();
            lon = loc.getLongitude();
            alt = loc.hasAltitude() ? loc.getAltitude() : 0;
            acc = loc.hasAccuracy() ? Math.max(1, loc.getAccuracy()) : 10;
        }
        s.time = now;
        csv.append(s.toCsv(lat, lon, alt, acc));

        if (!first) return;
        s.isNew = true;
        int ti = typeIndex(s.type);
        uniq[ti]++;
        int m = (int) ((now - startMs) / 60000);
        if (m >= 0 && m < BIN_MAX) bins[m][ti]++;
        if (s.type == Sighting.WIFI) {
            bands[Sighting.band(s.freq)]++;
            int sc = Sighting.secClass(s.auth);
            sec[sc]++;
            if (Sighting.isOpen(s.auth)) {
                openNets++;
                if (Prefs.b(Prefs.BUZZ_OPEN)) buzz();
            }
        }
        if (s.rssi > strongestRssi && s.type != Sighting.CELL) {
            strongestRssi = s.rssi;
            strongestName = s.name == null || s.name.length() == 0 ? s.mac : s.name;
        }
        if (allTimeLoaded && allTime.add(k)) { newEver++; s.newEver = true; }
        synchronized (feed) {
            feed.addFirst(s);
            while (feed.size() > 60) feed.pollLast();
        }
        if (lat != 0 || lon != 0) addDot(lat, lon, ti);
    }

    private void addDot(double lat, double lon, int ti) {
        if (dotN >= DOT_MAX) return;
        if (dotN == dotLat.length) {
            int n = Math.min(DOT_MAX, dotN * 2);
            dotLat = java.util.Arrays.copyOf(dotLat, n);
            dotLon = java.util.Arrays.copyOf(dotLon, n);
            dotType = java.util.Arrays.copyOf(dotType, n);
        }
        dotLat[dotN] = lat;
        dotLon[dotN] = lon;
        dotType[dotN] = (byte) ti;
        dotN++;
    }

    public static int typeIndex(char t) {
        switch (t) {
            case Sighting.BLE: return 1;
            case Sighting.BT: return 2;
            case Sighting.CELL: return 3;
            default: return 0;
        }
    }

    private void buzz() {
        try {
            Vibrator v = (Vibrator) ctx.getSystemService(Context.VIBRATOR_SERVICE);
            if (v != null) v.vibrate(VibrationEffect.createWaveform(new long[]{0, 60, 80, 60}, -1));
        } catch (Throwable ignored) { }
    }

    // ======================================================================
    //  Wi-Fi
    // ======================================================================
    private void startWifi() {
        wifi = (WifiManager) ctx.getSystemService(Context.WIFI_SERVICE);
        if (wifi == null) { wifiState = "No Wi-Fi on this phone"; return; }
        IntentFilter f = new IntentFilter(WifiManager.SCAN_RESULTS_AVAILABLE_ACTION);
        try {
            if (Build.VERSION.SDK_INT >= 33) ctx.registerReceiver(wifiRx, f, Context.RECEIVER_EXPORTED);
            else ctx.registerReceiver(wifiRx, f);
            wifiReg = true;
        } catch (Throwable t) {
            wifiState = "Can't listen for scans";
            return;
        }
        App.main.post(wifiTick);
    }

    private void stopWifi() {
        if (wifiReg) { try { ctx.unregisterReceiver(wifiRx); } catch (Throwable ignored) { } wifiReg = false; }
    }

    private final Runnable wifiTick = new Runnable() {
        @Override public void run() {
            if (!running || wifi == null) return;
            boolean canScan = wifi.isWifiEnabled() || wifi.isScanAlwaysAvailable();
            if (!canScan) {
                wifiState = "Wi-Fi is off";
            } else if (!locationServicesOn()) {
                wifiState = "Location is off";
            } else {
                wifiAsked++;
                boolean ok;
                try { ok = wifi.startScan(); } catch (Throwable t) { ok = false; }
                if (ok) {
                    wifiAccepted++;
                    acceptedTimes.addLast(SystemClock.elapsedRealtime());
                    wifiState = "Scanning";
                } else {
                    wifiRefused++;
                    wifiState = "Android is limiting scans";
                }
            }
            App.main.postDelayed(this, Math.max(1000, Prefs.i(Prefs.WIFI_INTERVAL)));
        }
    };

    /** True when Android has refused most recent scan requests. */
    public boolean wifiThrottled() {
        return wifiAsked >= 6 && wifiRefused * 2 > wifiAsked;
    }

    private final BroadcastReceiver wifiRx = new BroadcastReceiver() {
        @Override public void onReceive(Context c, Intent i) {
            if (!running || wifi == null) return;
            List<android.net.wifi.ScanResult> list;
            try { list = wifi.getScanResults(); } catch (Throwable t) { return; }
            if (list == null) return;
            wifiLastResultsAt = SystemClock.elapsedRealtime();
            long nowUs = SystemClock.elapsedRealtime() * 1000L;
            for (android.net.wifi.ScanResult r : list) {
                // skip cached results heard somewhere else: their position would be wrong
                if (r.timestamp > 0 && nowUs - r.timestamp > 20000000L) continue;
                if (r.BSSID == null) continue;
                Sighting s = new Sighting();
                s.type = Sighting.WIFI;
                s.mac = r.BSSID.toLowerCase(java.util.Locale.US);
                String ssid = r.SSID == null ? "" : r.SSID;
                if (ssid.length() >= 2 && ssid.startsWith("\"") && ssid.endsWith("\"")) ssid = ssid.substring(1, ssid.length() - 1);
                if ("<unknown ssid>".equals(ssid)) ssid = "";
                s.name = ssid;
                s.auth = r.capabilities == null || r.capabilities.length() == 0 ? "[ESS]" : r.capabilities;
                s.freq = r.frequency;
                s.channel = Sighting.freqToChannel(r.frequency);
                s.rssi = r.level;
                wifiResults++;
                handle(s);
            }
        }
    };

    // ======================================================================
    //  Bluetooth LE
    // ======================================================================
    private boolean btPerms() {
        if (Build.VERSION.SDK_INT >= 31) return has(ctx, Manifest.permission.BLUETOOTH_SCAN);
        return true;
    }

    private void startBle() {
        BluetoothManager bm = (BluetoothManager) ctx.getSystemService(Context.BLUETOOTH_SERVICE);
        bt = bm == null ? null : bm.getAdapter();
        if (bt == null) { bleState = "No Bluetooth on this phone"; return; }
        if (!btPerms()) { bleState = "Nearby devices permission is off"; return; }
        if (!bt.isEnabled()) { bleState = "Bluetooth is off"; App.main.postDelayed(bleRestart, 5000); return; }
        le = bt.getBluetoothLeScanner();
        if (le == null) { bleState = "Bluetooth is off"; App.main.postDelayed(bleRestart, 5000); return; }
        int mode = Prefs.i(Prefs.BLE_MODE);
        ScanSettings.Builder sb = new ScanSettings.Builder()
                .setScanMode(mode == 0 ? ScanSettings.SCAN_MODE_LOW_POWER : mode == 1 ? ScanSettings.SCAN_MODE_BALANCED : ScanSettings.SCAN_MODE_LOW_LATENCY)
                .setCallbackType(ScanSettings.CALLBACK_TYPE_ALL_MATCHES)
                .setReportDelay(0);
        try {
            if (bt.isLeExtendedAdvertisingSupported() || bt.isLeCodedPhySupported()) {
                sb.setLegacy(false);
                sb.setPhy(ScanSettings.PHY_LE_ALL_SUPPORTED);
            }
        } catch (Throwable ignored) { }
        // One empty filter: Android keeps delivering filtered scans with the screen off.
        List<ScanFilter> filters = new ArrayList<ScanFilter>();
        filters.add(new ScanFilter.Builder().build());
        try {
            le.startScan(filters, sb.build(), bleCb);
            bleOn = true;
            bleStartedAt = SystemClock.elapsedRealtime();
            bleState = "Scanning";
        } catch (Throwable t) {
            bleState = "Couldn't start: " + t.getMessage();
        }
        // Android quietly weakens a scan that runs for 30 minutes, so restart it every 10.
        App.main.postDelayed(bleRestart, 10 * 60000L);
    }

    private void stopBle() {
        if (bleOn && le != null) {
            try { le.stopScan(bleCb); } catch (Throwable ignored) { }
        }
        bleOn = false;
    }

    private final Runnable bleRestart = new Runnable() {
        @Override public void run() {
            if (!running || !Prefs.b(Prefs.BLE)) return;
            stopBle();
            App.main.postDelayed(new Runnable() {
                @Override public void run() { if (running) startBle(); }
            }, 1500);
        }
    };

    private final ScanCallback bleCb = new ScanCallback() {
        @Override public void onScanResult(int callbackType, ScanResult r) { onBle(r); }
        @Override public void onBatchScanResults(List<ScanResult> rs) { for (ScanResult r : rs) onBle(r); }
        @Override public void onScanFailed(int code) {
            bleOn = false;
            switch (code) {
                case SCAN_FAILED_ALREADY_STARTED: bleState = "Already scanning"; bleOn = true; return;
                case SCAN_FAILED_APPLICATION_REGISTRATION_FAILED: bleState = "Android refused (too many starts), retrying"; break;
                case SCAN_FAILED_FEATURE_UNSUPPORTED: bleState = "Not supported on this phone"; return;
                default: bleState = "Scan failed (" + code + "), retrying"; break;
            }
            App.main.removeCallbacks(bleRestart);
            App.main.postDelayed(bleRestart, 35000);
        }
    };

    private void onBle(ScanResult r) {
        if (r == null || r.getDevice() == null) return;
        bleResults++;
        Sighting s = new Sighting();
        s.type = Sighting.BLE;
        s.mac = r.getDevice().getAddress().toLowerCase(java.util.Locale.US);
        ScanRecord rec = r.getScanRecord();
        String name = rec == null ? null : rec.getDeviceName();
        s.name = name == null ? "" : name;
        s.auth = "Misc [LE]";
        s.rssi = r.getRssi();
        if (rec != null) {
            SparseArray<byte[]> m = rec.getManufacturerSpecificData();
            if (m != null && m.size() > 0) s.mfgr = m.keyAt(0);
        }
        handle(s);
    }

    // ======================================================================
    //  Bluetooth classic (discovery)
    // ======================================================================
    private void startBt() {
        BluetoothManager bm = (BluetoothManager) ctx.getSystemService(Context.BLUETOOTH_SERVICE);
        bt = bm == null ? null : bm.getAdapter();
        if (bt == null) { btState = "No Bluetooth on this phone"; return; }
        if (!btPerms()) { btState = "Nearby devices permission is off"; return; }
        btWanted = true;
        if (!btReg) {
            IntentFilter f = new IntentFilter(BluetoothDevice.ACTION_FOUND);
            f.addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED);
            try {
                if (Build.VERSION.SDK_INT >= 33) ctx.registerReceiver(btRx, f, Context.RECEIVER_EXPORTED);
                else ctx.registerReceiver(btRx, f);
                btReg = true;
            } catch (Throwable t) { btState = "Can't listen for devices"; return; }
        }
        discover();
    }

    private void discover() {
        if (!running || !btWanted || bt == null) return;
        if (!bt.isEnabled()) { btState = "Bluetooth is off"; App.main.postDelayed(btRestart, 5000); return; }
        try {
            if (!bt.isDiscovering() && !bt.startDiscovery()) {
                btState = "Discovery refused, retrying";
                App.main.postDelayed(btRestart, 10000);
            } else btState = "Searching";
        } catch (Throwable t) { btState = "Discovery failed"; }
    }

    private final Runnable btRestart = new Runnable() {
        @Override public void run() { discover(); }
    };

    private void stopBt() {
        btWanted = false;
        try { if (bt != null && bt.isDiscovering()) bt.cancelDiscovery(); } catch (Throwable ignored) { }
        if (btReg) { try { ctx.unregisterReceiver(btRx); } catch (Throwable ignored) { } btReg = false; }
    }

    private final BroadcastReceiver btRx = new BroadcastReceiver() {
        @Override public void onReceive(Context c, Intent i) {
            if (BluetoothAdapter.ACTION_DISCOVERY_FINISHED.equals(i.getAction())) {
                App.main.postDelayed(btRestart, 1500);
                return;
            }
            BluetoothDevice d = i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
            if (d == null) return;
            btResults++;
            Sighting s = new Sighting();
            s.type = Sighting.BT;
            s.mac = d.getAddress().toLowerCase(java.util.Locale.US);
            String n = i.getStringExtra(BluetoothDevice.EXTRA_NAME);
            s.name = n == null ? "" : n;
            s.auth = "Misc [BT]";
            s.rssi = i.getShortExtra(BluetoothDevice.EXTRA_RSSI, (short) -100);
            handle(s);
        }
    };

    // ======================================================================
    //  Cell towers
    // ======================================================================
    private void startCell() {
        tm = (TelephonyManager) ctx.getSystemService(Context.TELEPHONY_SERVICE);
        if (tm == null) { cellState = "No phone radio"; return; }
        cellState = "Listening";
        App.main.post(cellTick);
    }

    private final Runnable cellTick = new Runnable() {
        @Override public void run() {
            if (!running || tm == null) return;
            try {
                if (Build.VERSION.SDK_INT >= 29) Cells29.request(ScanEngine.this, tm, ctx);
                else onCells(tm.getAllCellInfo());
            } catch (Throwable t) {
                cellState = "Can't read cells";
            }
            App.main.postDelayed(this, 10000);
        }
    };

    void onCells(List<CellInfo> list) {
        if (!running || list == null) return;
        String iso = "";
        try { iso = tm.getNetworkCountryIso(); } catch (Throwable ignored) { }
        String op = "";
        try { op = tm.getNetworkOperatorName(); } catch (Throwable ignored) { }
        for (CellInfo ci : list) {
            Sighting s = null;
            try {
                if (ci instanceof CellInfoLte) {
                    CellIdentityLte id = ((CellInfoLte) ci).getCellIdentity();
                    s = cell("LTE", mcc(id), mnc(id), id.getTac(), id.getCi(), id.getEarfcn(),
                            ((CellInfoLte) ci).getCellSignalStrength().getDbm(), alpha(id, ci.isRegistered() ? op : ""), iso);
                } else if (ci instanceof CellInfoGsm) {
                    CellIdentityGsm id = ((CellInfoGsm) ci).getCellIdentity();
                    s = cell("GSM", mcc(id), mnc(id), id.getLac(), id.getCid(), id.getArfcn(),
                            ((CellInfoGsm) ci).getCellSignalStrength().getDbm(), alpha(id, ci.isRegistered() ? op : ""), iso);
                } else if (ci instanceof CellInfoWcdma) {
                    CellIdentityWcdma id = ((CellInfoWcdma) ci).getCellIdentity();
                    s = cell("WCDMA", mcc(id), mnc(id), id.getLac(), id.getCid(), id.getUarfcn(),
                            ((CellInfoWcdma) ci).getCellSignalStrength().getDbm(), alpha(id, ci.isRegistered() ? op : ""), iso);
                } else if (Build.VERSION.SDK_INT >= 29) {
                    s = Cells29.nr(this, ci, ci.isRegistered() ? op : "", iso);
                }
            } catch (Throwable ignored) { }
            if (s != null) { cellResults++; handle(s); }
        }
        cellState = "Listening";
    }

    Sighting cell(String type, String mcc, String mnc, long area, long id, int chan, int dbm, String name, String iso) {
        if (mcc == null || mnc == null || mcc.length() == 0 || mnc.length() == 0) return null;
        if (id <= 0 || id == Integer.MAX_VALUE || id == Long.MAX_VALUE || area == Integer.MAX_VALUE) return null;
        Sighting s = new Sighting();
        s.type = Sighting.CELL;
        s.cellType = type;
        s.mac = mcc + mnc + "_" + area + "_" + id;
        s.name = name == null ? "" : name;
        s.auth = type + ";" + (iso == null ? "" : iso);
        s.channel = chan == Integer.MAX_VALUE ? 0 : chan;
        s.rssi = dbm == Integer.MAX_VALUE ? -113 : dbm;
        return s;
    }

    @SuppressWarnings("deprecation")
    private static String mcc(Object id) {
        if (Build.VERSION.SDK_INT >= 28) {
            if (id instanceof CellIdentityLte) return ((CellIdentityLte) id).getMccString();
            if (id instanceof CellIdentityGsm) return ((CellIdentityGsm) id).getMccString();
            if (id instanceof CellIdentityWcdma) return ((CellIdentityWcdma) id).getMccString();
            return null;
        }
        int v = id instanceof CellIdentityLte ? ((CellIdentityLte) id).getMcc()
                : id instanceof CellIdentityGsm ? ((CellIdentityGsm) id).getMcc()
                : id instanceof CellIdentityWcdma ? ((CellIdentityWcdma) id).getMcc() : Integer.MAX_VALUE;
        return v == Integer.MAX_VALUE ? null : String.valueOf(v);
    }

    @SuppressWarnings("deprecation")
    private static String mnc(Object id) {
        if (Build.VERSION.SDK_INT >= 28) {
            if (id instanceof CellIdentityLte) return ((CellIdentityLte) id).getMncString();
            if (id instanceof CellIdentityGsm) return ((CellIdentityGsm) id).getMncString();
            if (id instanceof CellIdentityWcdma) return ((CellIdentityWcdma) id).getMncString();
            return null;
        }
        int v = id instanceof CellIdentityLte ? ((CellIdentityLte) id).getMnc()
                : id instanceof CellIdentityGsm ? ((CellIdentityGsm) id).getMnc()
                : id instanceof CellIdentityWcdma ? ((CellIdentityWcdma) id).getMnc() : Integer.MAX_VALUE;
        if (v == Integer.MAX_VALUE) return null;
        return v < 10 ? "0" + v : String.valueOf(v);
    }

    private static String alpha(android.telephony.CellIdentity id, String fallback) {
        if (Build.VERSION.SDK_INT >= 28) {
            CharSequence a = id.getOperatorAlphaLong();
            if (a != null && a.length() > 0) return a.toString();
        }
        return fallback;
    }

    // ======================================================================
    //  All-time list of everything this phone has logged
    // ======================================================================
    private File allTimeFile() { return new File(ctx.getFilesDir(), "seen.bin"); }

    private void loadAllTime() {
        if (allTimeLoaded || ctx == null) return;
        final File f = allTimeFile();
        App.io.post(new Runnable() {
            @Override public void run() {
                final HashSet<Long> got = new HashSet<Long>();
                if (f.exists()) {
                    DataInputStream in = null;
                    try {
                        in = new DataInputStream(new BufferedInputStream(new FileInputStream(f), 65536));
                        int n = in.readInt();
                        for (int i = 0; i < n; i++) got.add(in.readLong());
                    } catch (Throwable ignored) {
                    } finally {
                        try { if (in != null) in.close(); } catch (Throwable ignored) { }
                    }
                }
                App.main.post(new Runnable() {
                    @Override public void run() {
                        allTime.addAll(got);
                        allTimeLoaded = true;
                    }
                });
            }
        });
    }

    private void saveAllTime() {
        if (!allTimeLoaded || ctx == null) return;
        final long[] arr = new long[allTime.size()];
        int i = 0;
        for (Long l : allTime) arr[i++] = l;
        final File f = allTimeFile();
        App.io.post(new Runnable() {
            @Override public void run() {
                File tmp = new File(f.getPath() + ".tmp");
                DataOutputStream out = null;
                try {
                    out = new DataOutputStream(new BufferedOutputStream(new FileOutputStream(tmp), 65536));
                    out.writeInt(arr.length);
                    for (long v : arr) out.writeLong(v);
                    out.close();
                    out = null;
                    if (!tmp.renameTo(f)) { f.delete(); tmp.renameTo(f); }
                } catch (Throwable ignored) {
                } finally {
                    try { if (out != null) out.close(); } catch (Throwable ignored) { }
                }
            }
        });
    }

    public void clearAllTime() {
        allTime.clear();
        allTimeLoaded = true;
        if (ctx != null) allTimeFile().delete();
    }
}
