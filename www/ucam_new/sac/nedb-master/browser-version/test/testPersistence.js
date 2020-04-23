console.log("Beginning tests");
console.log("Please note these tests work on Chrome latest, might not work on other browsers due to discrepancies in how local storage works for the file:// protocol");

function testsFailed () {
  document.getElementById("results").innerHTML = "TESTS FAILED";
}

var filename = 'c:/tests/test';
alert(111);
var db = new Nedb({ filename: filename, autoload: true });
alert(11);
db.remove({}, { multi: true }, function () {
  db.insert({ hello: 'world' }, function (err) {
    if (err) {
      testsFailed();
      return;
    }

    window.location = './testPersistence2.html';
  });
});
