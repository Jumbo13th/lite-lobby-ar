import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';
import { remarkHeadingId } from 'remark-custom-heading-id';
import { unified } from '@astrojs/markdown-remark';
import chapters from './src/data/chapters.json' with { type: 'json' };

function guideGroup(slug) {
  const hub = chapters.hubs.find((page) => page.slug === slug);
  return {
    label: hub.title.en,
    translations: { ru: hub.title.ru },
    collapsed: true,
    items: [
      { slug, label: 'Introduction', translations: { ru: 'Введение' } },
      ...chapters.parts.map((part) => ({
        label: part.title.en,
        translations: { ru: part.title.ru },
        collapsed: true,
        items: chapters.sequence
          .filter((route) => chapters.chapters.some((chapter) => chapter.slug === route && chapter.part === part.id))
          .map((route) => ({ slug: route })),
      })),
    ],
  };
}

export default defineConfig({
  site: 'https://jumbo13th.github.io',
  base: '/lite-lobby-ar',
  trailingSlash: 'always',
  image: { responsiveStyles: true },
  markdown: { processor: unified({ remarkPlugins: [remarkHeadingId] }) },
  integrations: [
    starlight({
      title: 'Lite Lobby',
      social: [
        { icon: 'discord', label: 'Discord', href: 'https://discord.gg/t8TK9Y2vsM' },
        { icon: 'github', label: 'GitHub', href: 'https://github.com/Jumbo13th/lite-lobby-ar' },
        { icon: 'telegram', label: 'Telegram', href: 'https://t.me/triad_tactics' },
      ],
      defaultLocale: 'root',
      locales: {
        root: { label: 'English', lang: 'en' },
        ru: { label: 'Русский', lang: 'ru' },
      },
      sidebar: [
        { slug: 'index', label: 'Guide', translations: { ru: 'Руководство' } },
        guideGroup('create-mission'),
        {
          label: 'Additional material',
          translations: { ru: 'Дополнительные материалы' },
          collapsed: true,
          items: [
            { slug: 'example-mission' },
            { slug: 'git' },
            { slug: 'character-prefabs/planning' },
            { slug: 'character-prefabs/prefab-operations' },
          ],
        },
      ],
      tableOfContents: { minHeadingLevel: 2, maxHeadingLevel: 3 },
      pagination: false,
      components: {
        Header: './src/components/LL_Header.astro',
        Hero: './src/components/LL_HomeHero.astro',
        SocialIcons: './src/components/LL_SocialIcons.astro',
        Footer: './src/components/LL_Footer.astro',
      },
      expressiveCode: {
        styleOverrides: { borderRadius: '0.5rem', codeFontSize: '0.875rem' },
      },
      customCss: ['./src/styles/custom.css'],
    }),
  ],
});
