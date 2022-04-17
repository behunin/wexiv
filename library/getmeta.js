mergeInto(LibraryManager.library, {
    $DBStore: {
        indexedDB: function () {
            if (typeof indexedDB !== 'undefined') return indexedDB;
            var ret = null;
            if (typeof window === 'object') ret = window.indexedDB;
            assert(ret, 'indexedDB not supported');
            return ret;
        },
        DB_VERSION: 2,
        DB_STORE_NAME: "wexiv",
        setIndex: function (data) {
            try {
                var req = DBStore.indexedDB().open(DBStore.DB_STORE_NAME, DBStore.DB_VERSION);
                req.onupgradeneeded = function (ev) {
                    var db = ev.target.result;
                    var transaction = ev.target.transaction;
                    var store;
                    if (db.objectStoreNames.contains(DBStore.DB_STORE_NAME)) {
                        store = transaction.objectStore(DBStore.DB_STORE_NAME);
                    } else {
                        store = db.createObjectStore(DBStore.DB_STORE_NAME, { keyPath: "name" });
                    }
                    store.createIndex("exif", "exif", { unique: false });
                    store.createIndex("iptc", "iptc", { unique: false });
                    store.createIndex("xmp", "xmp", { unique: false });
                    store.createIndex("head", "head", { unique: false });
                };
                req.onsuccess = function (event) {
                    var db = req.result;
                    var transaction = db.transaction(DBStore.DB_STORE_NAME, "readwrite");
                    var store = transaction.objectStore(DBStore.DB_STORE_NAME);
                    var reqS = store.put(data);
                };
            } catch (e) {
                console.error(e);
            }
        },
    },
    db_store: function (name, exif_handle, iptc_handle, xmp_handle, head_handle) {
        var exifdata = Emval.toValue(exif_handle);
        var iptcdata = Emval.toValue(iptc_handle);
        var xmpdata = Emval.toValue(xmp_handle);
        var headdata = Emval.toValue(head_handle);
        var str = UTF8ToString(name);
        var obj = { name: str, exif: exifdata, iptc: iptcdata, xmp: xmpdata, head: headdata };
        DBStore.setIndex(obj);
    },
    db_store__deps: ["$DBStore", "$Emval"]
});