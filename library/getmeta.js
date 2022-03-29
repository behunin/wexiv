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
        DB_STORE_NAME: 'Wexiv',
        db: {},
        getDB: function (callback) {
            if (DBStore.db.error && DBStore.db.error.name === "NoError") {
                console.log("DB Cache Success", DBStore.db.error.code);
                return callback(null, DBStore.db);
            }
            var req;
            try {
                req = DBStore.indexedDB().open(DBStore.DB_STORE_NAME, DBStore.DB_VERSION);
            } catch (e) {
                return callback(e);
            }
            req.onupgradeneeded = function (e) {
                var db = e.target.result;
                var transaction = e.target.transaction;
                var store;
                if (db.objectStoreNames.contains(DBStore.DB_STORE_NAME)) {
                    store = transaction.objectStore(DBStore.DB_STORE_NAME);
                } else {
                    store = db.createObjectStore(DBStore.DB_STORE_NAME, { keyPath: "name" });
                    store.createIndex("exif", "exif", { unique: false });
                    store.createIndex("iptc", "iptc", { unique: false });
                    store.createIndex("xmp", "xmp", { unique: false });
                    store.createIndex("head", "head", { unique: false });
                }
            };
            req.onsuccess = function () {
                DBStore.db = req.result;
                callback(null, DBStore.db);
            };
            req.onerror = function (e) {
                callback(this.error);
                e.preventDefault();
            };
        },
        getStore: function (type, callback) {
            DBStore.getDB(function (error, db) {
                if (error) return callback(error);
                var transaction = db.transaction([DBStore.DB_STORE_NAME], type);
                transaction.onerror = function (e) {
                    callback(this.error || 'unknown error');
                    e.preventDefault();
                };
                var store = transaction.objectStore(DBStore.DB_STORE_NAME);
                callback(null, store);
            });
        },
        setIndex: function (data, callback) {
            DBStore.getStore('readwrite', function (err, store) {
                if (err) return callback(err);
                var req = store.put(data);
                req.onsuccess = function (event) {
                    callback();
                };
                req.onerror = function (error) {
                    callback(error);
                };
            });
        },
    },
    db_store: function (name, exif_handle, iptc_handle, xmp_handle, head_handle) {
        var exifdata = Emval.toValue(exif_handle);
        var iptcdata = Emval.toValue(iptc_handle);
        var xmpdata = Emval.toValue(xmp_handle);
        var headdata = Emval.toValue(head_handle);
        var str = UTF8ToString(name);
        var obj = { name: str, exif: exifdata, iptc: iptcdata, xmp: xmpdata, head: headdata };
        DBStore.setIndex(obj, function (error) {
            if (error) {
                console.error(error);
            }
        });
    },
    db_store__deps: ["$DBStore", "$Emval"]
});