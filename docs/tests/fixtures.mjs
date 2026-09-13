export const fixtureSlug = 'll-validation-fixture';

export const fixtureText = {
  en: {
    title: 'Article layout preview',
    intro: 'This temporary article previews the guide layout. It is excluded from the published website and does not describe mission creation steps.',
    preparation: 'Before you begin',
    preparationText: 'A guide needs room for a short introduction, a few requirements, and a clear explanation of the result. This sample checks how those elements read alongside screenshots.',
    list: ['A concise introduction to the topic.', 'Short requirements that are easy to scan.', 'Images with captions and access to the original.'],
    details: 'Details and examples',
    detailsText: 'Small reference tables and code examples should remain readable without interrupting the flow of the article. Inline code such as `LL_Screenshot` follows the same visual style.',
    tableHead: ['Element', 'Purpose'],
    tableRows: [['Section heading', 'Find a topic in the page contents'], ['Caption', 'Explain what the image shows']],
    noteTitle: 'Layout sample',
    note: 'This text is for design review only. The actual guides will be written together in the content phase.',
    screenshots: 'Screenshots',
    screenshotsText: 'A small dialog keeps its natural size. A full scene fits the available width; both images link to the unchanged original.',
    smallAlt: 'Rename File dialog with a character prefab filename',
    smallCaption: 'The file rename dialog.',
    largeAlt: 'Mission preview in Game Master showing a squad on a grassy hillside',
    largeCaption: 'A squad in the mission preview.',
    originalLabel: 'Open original image',
  },
  ru: {
    title: 'Предпросмотр оформления статьи',
    intro: 'Эта временная статья нужна для проверки оформления руководств. Она не попадёт на опубликованный сайт и не содержит инструкций по созданию миссии.',
    preparation: 'Перед началом',
    preparationText: 'В руководстве нужны короткое введение, список требований и понятное описание результата. Этот пример показывает, как текст будет выглядеть рядом со скриншотами.',
    list: ['Короткое введение в тему.', 'Список требований, который легко просмотреть.', 'Изображения с подписями и ссылками на оригинал.'],
    details: 'Детали и примеры',
    detailsText: 'Небольшие таблицы и примеры кода должны оставаться читаемыми и не мешать чтению статьи. Код внутри строки, например `LL_Screenshot`, оформлен в том же стиле.',
    tableHead: ['Элемент', 'Назначение'],
    tableRows: [['Заголовок раздела', 'Поиск темы в содержании страницы'], ['Подпись', 'Пояснение к изображению']],
    noteTitle: 'Образец оформления',
    note: 'Этот текст нужен только для проверки дизайна. Сами руководства мы напишем вместе на этапе работы над содержанием.',
    screenshots: 'Скриншоты',
    screenshotsText: 'Небольшое окно сохраняет исходный размер. Общий вид миссии подстраивается под ширину страницы. У обоих изображений есть ссылка на оригинал.',
    smallAlt: 'Окно Rename File с именем файла префаба персонажа',
    smallCaption: 'Окно переименования файла.',
    largeAlt: 'Предпросмотр миссии в Game Master: группа бойцов на травянистом склоне',
    largeCaption: 'Группа бойцов в предпросмотре миссии.',
    originalLabel: 'Открыть оригинал изображения',
  },
};

export function fixtureSource(locale) {
  const text = fixtureText[locale];
  const prefix = locale === 'ru' ? '../../../' : '../../';
  return `---
title: ${text.title}
description: ${text.title}
---

import LL_Screenshot from '${prefix}components/LL_Screenshot.astro';
import { Aside } from '@astrojs/starlight/components';
import small from '${prefix}assets/guide/51.png';
import large from '${prefix}assets/guide/78.png';

${text.intro}

## ${text.preparation} {#preparation}

${text.preparationText}

${text.list.map((item) => `- ${item}`).join('\n')}

## ${text.details} {#details}

${text.detailsText}

| ${text.tableHead.join(' | ')} |
| --- | --- |
${text.tableRows.map((row) => `| ${row.join(' | ')} |`).join('\n')}

<Aside type="note" title=${JSON.stringify(text.noteTitle)}>
  ${text.note}
</Aside>

\`\`\`typescript
const screenshotValidation = true;
\`\`\`

## ${text.screenshots} {#screenshots}

${text.screenshotsText}

<LL_Screenshot src={small} alt=${JSON.stringify(text.smallAlt)} caption=${JSON.stringify(text.smallCaption)} originalLabel=${JSON.stringify(text.originalLabel)} />

<LL_Screenshot src={large} alt=${JSON.stringify(text.largeAlt)} caption=${JSON.stringify(text.largeCaption)} originalLabel=${JSON.stringify(text.originalLabel)} />
`;
}
