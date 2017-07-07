import { WebSrcPage } from './app.po';

describe('web-src App', () => {
  let page: WebSrcPage;

  beforeEach(() => {
    page = new WebSrcPage();
  });

  it('should display welcome message', () => {
    page.navigateTo();
    expect(page.getParagraphText()).toEqual('Welcome to app!!');
  });
});
