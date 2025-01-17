(async function() {
  TestRunner.addResult(
      'Verifies that BinaryResourceViewFactory interprets base64 data correctly');
  TestRunner.addResult('');

  await TestRunner.loadModule('source_frame');
  const base64content =
      'c2VuZGluZyB0aGlzIHV0Zi04IHN0cmluZyBhcyBhIGJpbmFyeSBtZXNzYWdlLi4u';
  const factory = new SourceFrame.BinaryResourceViewFactory(
      base64content, 'http://example.com', Common.resourceTypes.WebSocket);

  TestRunner.addResult('Base64View:');
  TestRunner.addResult(await (factory.createBase64View()._lazyContent()));
  TestRunner.addResult('');

  TestRunner.addResult('HexView:');
  TestRunner.addResult(await (factory.createHexView()._lazyContent()));
  TestRunner.addResult('');

  TestRunner.addResult('HexViewerView:');
  TestRunner.addResult(await (factory.createHexViewerView()._lazyContent()));
  TestRunner.addResult('');

  TestRunner.addResult('Utf8View:');
  TestRunner.addResult(await (factory.createUtf8View()._lazyContent()));
  TestRunner.addResult('');

  TestRunner.completeTest();
})();
