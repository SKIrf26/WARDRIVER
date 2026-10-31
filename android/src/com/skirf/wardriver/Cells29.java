package com.skirf.wardriver;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.telephony.CellIdentityNr;
import android.telephony.CellInfo;
import android.telephony.CellInfoNr;
import android.telephony.TelephonyManager;

import java.util.List;

/** Android 10+ only: fresh cell readings and 5G (NR) cells. Kept apart so older phones never load it. */
@TargetApi(29)
@SuppressLint("MissingPermission")
final class Cells29 {
    static void request(final ScanEngine eng, TelephonyManager tm, Context ctx) {
        tm.requestCellInfoUpdate(ctx.getMainExecutor(), new TelephonyManager.CellInfoCallback() {
            @Override public void onCellInfo(List<CellInfo> list) { eng.onCells(list); }
            @Override public void onError(int code, Throwable detail) { }
        });
    }

    static Sighting nr(ScanEngine eng, CellInfo ci, String op, String iso) {
        if (!(ci instanceof CellInfoNr)) return null;
        CellIdentityNr id = (CellIdentityNr) ((CellInfoNr) ci).getCellIdentity();
        CharSequence a = id.getOperatorAlphaLong();
        String name = a != null && a.length() > 0 ? a.toString() : op;
        int dbm = ((CellInfoNr) ci).getCellSignalStrength().getDbm();
        return eng.cell("NR", id.getMccString(), id.getMncString(), id.getTac(), id.getNci(), id.getNrarfcn(), dbm, name, iso);
    }

    private Cells29() {}
}
